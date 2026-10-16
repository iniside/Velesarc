#include "DataExpression.h"
#include "../../Include/RmlUi/Core/ConstantDataExpression.h"
#include "../../Include/RmlUi/Core/DataModelHandle.h"
#include "../../Include/RmlUi/Core/DataTypeRegister.h"
#include "../../Include/RmlUi/Core/Event.h"
#include "../../Include/RmlUi/Core/Variant.h"
#include "ArcPerfInternal.h"
#include "DataModel.h"
#include "XMLParseTools.h"
#include <stack>
#include <string.h>

#ifdef _MSC_VER
	#pragma warning(default : 4061)
	#pragma warning(default : 4062)
#endif

namespace Rml {

class DataParser;

/*
    The abstract machine for RmlUi data expressions.

    The machine can execute a program which contains a list of instructions listed below.

    The abstract machine has three registers:
        R  Typically results and right-hand side arguments.
        L  Typically left-hand side arguments.

    And a stack:
        S  The program stack.

    In addition, each instruction has an optional payload:
        D  Instruction data (payload).

    Notation used in the instruction list below:
        S+  Push to stack S.
        S-  Pop stack S (returns the popped value).
*/
enum class Instruction {
	// clang-format off
	// Assignment (register/stack) = Read (register R/L, instruction data D, or stack)
	Push            = 'P',     //      S+ = R
	Pop             = 'o',     //   <R/L> = S-  (D determines R/L)
	Literal         = 'D',     //       R = D
	Variable        = 'V',     //       R = DataModel.GetVariable(D)  (D is an index into the variable address list)
	Add             = '+',     //       R = L + R
	Subtract        = '-',     //       R = L - R
	Multiply        = '*',     //       R = L * R
	Divide          = '/',     //       R = L / R
	Not             = '!',     //       R = !R
	And             = '&',     //       R = L && R
	Or              = '|',     //       R = L || R
	Less            = '<',     //       R = L < R
	LessEq          = 'L',     //       R = L <= R
	Greater         = '>',     //       R = L > R
	GreaterEq       = 'G',     //       R = L >= R
	Equal           = '=',     //       R = L == R
	NotEqual        = 'N',     //       R = L != R
	NumArguments    = '#',     //       R = D  (Contains the num. arguments currently on the stack, immediately followed by a 'T' or 'E' instruction)
	TransformFnc    = 'T',     //       R = DataModel.Execute(D, A) where A = S[TOP - R, TOP]; S -= R;  (D determines function name, input R the num. arguments, A the arguments)
	EventFnc        = 'E',     //       DataModel.EventCallback(D, A); S -= R;
	Assign          = 'A',     //       DataModel.SetVariable(D, R)
	DynamicVariable = 'Y',     //       DataModel.GetVariable(DataModel.ParseAddress(R)) (Looks up a variable by path in R)
	CastToInt       = 'I',     //       R = (int)R
	Jump            = 'J',     //       Jumps to instruction index D
	JumpIfZero      = 'Z',     //       If R is false, jumps to instruction index D
	// clang-format on
};

enum class Register {
	R,
	L,
};

struct InstructionData {
	Instruction instruction;
	Variant data;
};

struct ProgramState {
	size_t program_length;
	int stack_size;
};

namespace Parse {
	static void Assignment(DataParser& parser);
	static void Expression(DataParser& parser);
} // namespace Parse

class DataParser {
public:
	DataParser(const String& expression, DataExpressionInterface expression_interface) : expression(expression), expression_interface(expression_interface)
	{}

	char Look()
	{
		if (reached_end)
			return '\0';
		return expression[index];
	}

	bool Match(char c, bool skip_whitespace = true)
	{
		if (c == Look())
		{
			Next();
			if (skip_whitespace)
				SkipWhitespace();
			return true;
		}
		Expected(c);
		return false;
	}

	char Next()
	{
		++index;
		if (index >= expression.size())
			reached_end = true;
		return Look();
	}

	void SkipWhitespace()
	{
		char c = Look();
		while (StringUtilities::IsWhitespace(c))
			c = Next();
	}

	void Error(const String& message)
	{
		parse_error = true;
		if (constant_only || model_free)
			return;
		Log::Message(Log::LT_WARNING, "Error in data expression at %zu. %s", index, message.c_str());
		Log::Message(Log::LT_WARNING, "  \"%s\"", expression.c_str());

		const size_t cursor_offset = size_t(index) + 3;
		const String cursor_string = String(cursor_offset, ' ') + '^';
		Log::Message(Log::LT_WARNING, "%s", cursor_string.c_str());
	}
	void Expected(const String& expected_symbols)
	{
		const char c = Look();
		if (c == '\0')
			Error(CreateString("Expected %s but found end of string.", expected_symbols.c_str()));
		else
			Error(CreateString("Expected %s but found character '%c'.", expected_symbols.c_str(), c));
	}
	void Expected(char expected) { Expected(String(1, '\'') + expected + '\''); }

	bool Parse(bool is_assignment_expression)
	{
		program.clear();
		variable_addresses.clear();
		address_names.clear();
		all_addresses_resolved = true;
		index = 0;
		reached_end = false;
		parse_error = false;
		if (expression.empty())
			reached_end = true;

		SkipWhitespace();

		if (is_assignment_expression)
			Parse::Assignment(*this);
		else
			Parse::Expression(*this);

		if (!reached_end)
		{
			parse_error = true;
			Error(CreateString("Unexpected character '%c' encountered.", Look()));
		}
		if (!parse_error && program_stack_size != 0)
		{
			parse_error = true;
			Error(CreateString("Internal parser error, inconsistent stack operations. Stack size is %d at parse end.", program_stack_size));
		}

		return !parse_error;
	}

	Program ReleaseProgram()
	{
		RMLUI_ASSERT(!parse_error);
		return std::move(program);
	}
	AddressList ReleaseAddresses()
	{
		RMLUI_ASSERT(!parse_error);
		return std::move(variable_addresses);
	}
	StringList ReleaseAddressNames()
	{
		RMLUI_ASSERT(!parse_error);
		return std::move(address_names);
	}

	void Emit(Instruction instruction, Variant data = Variant())
	{
		RMLUI_ASSERTMSG(instruction != Instruction::Push && instruction != Instruction::Pop && instruction != Instruction::NumArguments &&
				instruction != Instruction::TransformFnc && instruction != Instruction::EventFnc && instruction != Instruction::Variable &&
				instruction != Instruction::Assign,
			"Use Push(), Pop(), Function(), Variable(), and Assign() procedures for stack manipulation and variable instructions.");

		program.push_back(InstructionData{instruction, std::move(data)});
	}
	void Push()
	{
		program_stack_size += 1;
		program.push_back(InstructionData{Instruction::Push, Variant()});
	}
	void Pop(Register destination)
	{
		if (program_stack_size <= 0)
		{
			Error("Internal parser error: Tried to pop an empty stack.");
			return;
		}
		program_stack_size -= 1;
		program.push_back(InstructionData{Instruction::Pop, Variant(int(destination))});
	}
	void Function(Instruction instruction, int num_arguments, String&& name)
	{
		RMLUI_ASSERT(instruction == Instruction::TransformFnc || instruction == Instruction::EventFnc);
		RMLUI_ASSERT(num_arguments >= 0);
		if (program_stack_size < num_arguments)
		{
			Error(CreateString("Internal parser error: Popping %d arguments, but the stack contains only %d elements.", num_arguments,
				program_stack_size));
			return;
		}
		program_stack_size -= num_arguments;
		program.push_back(InstructionData{Instruction::NumArguments, Variant(int(num_arguments))});
		program.push_back(InstructionData{instruction, Variant(std::move(name))});
	}
	void Variable(const String& data_address) { VariableGetSet(data_address, false); }
	void Assign(const String& data_address) { VariableGetSet(data_address, true); }
	size_t InstructionIndex() const { return program.size(); }
	void PatchInstruction(size_t index, InstructionData data) { program[index] = data; }

	ProgramState GetProgramState() { return ProgramState{program.size(), program_stack_size}; }

	void SetProgramState(const ProgramState& state)
	{
		RMLUI_ASSERT(state.program_length <= program.size());
		program.resize(state.program_length);
		program_stack_size = state.stack_size;
	}

	void SetConstantOnly() { constant_only = true; }
	void SetModelFree() { model_free = true; }
	bool ReferencesVariable() const { return references_variable; }
	void SetRecordAddressNames() { record_address_names = true; }
	bool AllAddressesResolved() const { return all_addresses_resolved; }

	bool AddVariableAddress(const String& name)
	{
		if (constant_only)
		{
			references_variable = true;
			return false;
		}

		if (model_free)
		{
			address_names.push_back(name);
			return true;
		}

		DataAddress address = expression_interface.ParseAddress(name);
		if (address.empty())
		{
			all_addresses_resolved = false;
			return false;
		}

		variable_addresses.push_back(std::move(address));
		if (record_address_names)
			address_names.push_back(name);
		return true;
	}

private:
	void VariableGetSet(const String& name, bool is_assignment)
	{
		if (constant_only)
		{
			references_variable = true;
			return;
		}

		if (model_free)
		{
			const int index = int(address_names.size());
			address_names.push_back(name);
			program.push_back(InstructionData{is_assignment ? Instruction::Assign : Instruction::Variable, Variant(index)});
			return;
		}

		DataAddress address = expression_interface.ParseAddress(name);
		if (address.empty())
		{
			Error(CreateString("Could not find data variable with name '%s'.", name.c_str()));
			return;
		}
		int index = int(variable_addresses.size());
		variable_addresses.push_back(std::move(address));
		if (record_address_names)
			address_names.push_back(name);
		program.push_back(InstructionData{is_assignment ? Instruction::Assign : Instruction::Variable, Variant(int(index))});
	}

	const String& expression;
	DataExpressionInterface expression_interface;

	size_t index = 0;
	bool reached_end = false;
	bool parse_error = true;
	bool constant_only = false;
	bool model_free = false;
	bool references_variable = false;
	bool record_address_names = false;
	bool all_addresses_resolved = true;
	int program_stack_size = 0;

	Program program;

	AddressList variable_addresses;
	StringList address_names;
};

namespace Parse {

	// Forward declare all parse functions.
	static void Assignment(DataParser& parser);

	// The following in order of precedence.
	static void Expression(DataParser& parser);
	static void Relational(DataParser& parser);
	static void Additive(DataParser& parser);
	static void Term(DataParser& parser);
	static void Factor(DataParser& parser);

	static void NumberLiteral(DataParser& parser);
	static void StringLiteral(DataParser& parser);
	static String VariableExpression(DataParser& parser, const String& address_prefix);
	static void VariableOrFunction(DataParser& parser);

	static void Add(DataParser& parser);
	static void Subtract(DataParser& parser);
	static void Multiply(DataParser& parser);
	static void Divide(DataParser& parser);

	static void Not(DataParser& parser);
	static void And(DataParser& parser);
	static void Or(DataParser& parser);
	static void Less(DataParser& parser);
	static void Greater(DataParser& parser);
	static void Equal(DataParser& parser);
	static void NotEqual(DataParser& parser);

	static void Ternary(DataParser& parser);
	static void Function(DataParser& parser, Instruction function_type, String&& name, bool first_argument_piped);

	// Helper functions
	static bool IsVariableCharacter(char c, bool is_first_character)
	{
		const bool is_alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');

		if (is_first_character)
			return is_alpha;

		if (is_alpha || (c >= '0' && c <= '9'))
			return true;

		for (char valid_char : "_.")
		{
			if (c == valid_char && valid_char != '\0')
				return true;
		}

		return false;
	}
	static String VariableOrFunctionName(DataParser& parser, bool* out_valid_function_name)
	{
		String name;
		bool is_first_character = true;
		char c = parser.Look();

		while (IsVariableCharacter(c, is_first_character))
		{
			name += c;
			c = parser.Next();
			is_first_character = false;
		}

		if (out_valid_function_name)
			*out_valid_function_name = (name.find_first_of(". ") == String::npos);

		return name;
	}

	static String FindNumberLiteral(DataParser& parser, bool silent)
	{
		String str;

		bool first_match = false;
		bool has_dot = false;
		char c = parser.Look();
		if (c == '-')
		{
			str += c;
			c = parser.Next();
		}

		while ((c >= '0' && c <= '9') || (c == '.' && !has_dot))
		{
			first_match = true;
			str += c;
			if (c == '.')
				has_dot = true;
			c = parser.Next();
		}

		if (!first_match)
		{
			if (!silent)
				parser.Error(CreateString("Invalid number literal. Expected '0-9' or '.' but found '%c'.", c));

			return String();
		}

		return str;
	}

	// Parser functions
	static void Assignment(DataParser& parser)
	{
		bool looping = true;
		while (looping)
		{
			if (parser.Look() != '\0')
			{
				String variable_name = VariableOrFunctionName(parser, nullptr);
				if (variable_name.empty())
				{
					parser.Error("Expected a variable for assignment but got an empty name.");
					return;
				}

				parser.SkipWhitespace();

				const char c = parser.Look();
				if (c == '=')
				{
					parser.Match('=');
					Expression(parser);
					parser.Assign(variable_name);
				}
				else if (c == '(' || c == ';' || c == '\0')
				{
					Function(parser, Instruction::EventFnc, std::move(variable_name), false);
				}
				else
				{
					parser.Expected("one of  = ; (  or end of string");
					return;
				}
			}

			const char c = parser.Look();
			if (c == ';')
				parser.Match(';');
			else if (c == '\0')
				looping = false;
			else
			{
				parser.Expected("';' or end of string");
				looping = false;
			}
		}
	}
	static void Expression(DataParser& parser)
	{
		Relational(parser);

		bool looping = true;
		while (looping)
		{
			switch (parser.Look())
			{
			case '&': And(parser); break;
			case '|':
			{
				parser.Match('|', false);
				if (parser.Look() == '|')
					Or(parser);
				else
				{
					parser.Push();
					parser.SkipWhitespace();
					bool valid_function_name = true;
					String name = VariableOrFunctionName(parser, &valid_function_name);
					if (name.empty())
					{
						parser.Error("Expected a transform function name but got an empty name.");
						return;
					}
					if (!valid_function_name)
					{
						parser.Error("Expected a transform function name but got an invalid name '" + name + "'.");
						return;
					}

					Function(parser, Instruction::TransformFnc, std::move(name), true);
				}
			}
			break;
			case '?': Ternary(parser); break;
			default: looping = false;
			}
		}
	}

	static void Relational(DataParser& parser)
	{
		Additive(parser);

		bool looping = true;
		while (looping)
		{
			switch (parser.Look())
			{
			case '=': Equal(parser); break;
			case '!': NotEqual(parser); break;
			case '<': Less(parser); break;
			case '>': Greater(parser); break;
			default: looping = false;
			}
		}
	}

	static void Additive(DataParser& parser)
	{
		Term(parser);

		bool looping = true;
		while (looping)
		{
			switch (parser.Look())
			{
			case '+': Add(parser); break;
			case '-': Subtract(parser); break;
			default: looping = false;
			}
		}
	}

	static void Term(DataParser& parser)
	{
		Factor(parser);

		bool looping = true;
		while (looping)
		{
			switch (parser.Look())
			{
			case '*': Multiply(parser); break;
			case '/': Divide(parser); break;
			default: looping = false;
			}
		}
	}
	static void Factor(DataParser& parser)
	{
		const char c = parser.Look();

		if (c == '(')
		{
			parser.Match('(');
			Expression(parser);
			parser.Match(')');
		}
		else if (c == '\'')
		{
			parser.Match('\'', false);
			StringLiteral(parser);
			parser.Match('\'');
		}
		else if (c == '!')
		{
			Not(parser);
			parser.SkipWhitespace();
		}
		else if (c == '-' || (c >= '0' && c <= '9'))
		{
			NumberLiteral(parser);
			parser.SkipWhitespace();
		}
		else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
		{
			VariableOrFunction(parser);
			parser.SkipWhitespace();
		}
		else
			parser.Expected("literal, variable name, function name, parenthesis, or '!'");
	}

	static void NumberLiteral(DataParser& parser)
	{
		String str = FindNumberLiteral(parser, false);

		if (str.empty())
			return;

		const double number = FromString(str, 0.0);

		parser.Emit(Instruction::Literal, Variant(number));
	}

	static void StringLiteral(DataParser& parser)
	{
		String str;

		char c = parser.Look();
		char c_prev = '\0';

		while (c != '\0' && (c != '\'' || c_prev == '\\'))
		{
			if (c_prev == '\\' && (c == '\\' || c == '\''))
			{
				str.pop_back();
				c_prev = '\0';
			}
			else
			{
				c_prev = c;
			}

			str += c;
			c = parser.Next();
		}

		parser.Emit(Instruction::Literal, Variant(str));
	}

	static String VariableExpression(DataParser& parser, const String& address_prefix)
	{
		if (parser.Look() == '[')
		{
			parser.Next();
			String prefix = address_prefix;

			prefix.push_back('[');

			// Backup program state before trying to parse number inside brackets.
			// Could turn out to be expression and needs reparsing
			auto backup_state = parser.GetProgramState();

			String index = FindNumberLiteral(parser, true);
			if (!index.empty() && parser.Look() == ']')
			{
				parser.Next();
				prefix.append(index);
				prefix.push_back(']');
				return VariableExpression(parser, prefix);
			}
			else
			{
				parser.SetProgramState(backup_state);

				parser.Emit(Instruction::Literal, Variant(prefix));
				parser.Push();

				Expression(parser);
				parser.Emit(Instruction::CastToInt);
				parser.Push();

				parser.Match(']');
				VariableExpression(parser, "]");

				parser.Pop(Register::L);
				parser.Emit(Instruction::Add, Variant());
				parser.Pop(Register::L);
				parser.Emit(Instruction::Add, Variant());

				return "";
			}
		}
		else if (parser.Look() == '.')
		{
			parser.Next();
			return VariableExpression(parser, address_prefix + ".");
		}
		else
		{
			String next = VariableOrFunctionName(parser, nullptr);
			if (next.empty())
			{
				if (!address_prefix.empty())
					parser.Emit(Instruction::Literal, Variant(address_prefix));
				return address_prefix;
			}
			return VariableExpression(parser, address_prefix + next);
		}
	}

	static void VariableOrFunction(DataParser& parser)
	{
		bool valid_function_name = true;
		String name = VariableOrFunctionName(parser, &valid_function_name);
		if (name.empty())
		{
			parser.Error("Expected a variable or function name but got an empty name.");
			return;
		}

		// Keywords are parsed like variables, but are really literals. Check for them here.
		if (name == "true")
			parser.Emit(Instruction::Literal, Variant(true));
		else if (name == "false")
			parser.Emit(Instruction::Literal, Variant(false));
		else if (parser.Look() == '(')
		{
			if (!valid_function_name)
			{
				parser.Error("Invalid function name '" + name + "'.");
				return;
			}
			Function(parser, Instruction::TransformFnc, std::move(name), false);
		}
		else if (parser.Look() == '[')
		{
			// Backup program state before trying to parse part inside brackets.
			// Could turn out to be expression and needs reparsing
			auto backup_state = parser.GetProgramState();

			String full_address = VariableExpression(parser, name);

			if (!full_address.empty())
			{
				parser.SetProgramState(backup_state);
				parser.Variable(full_address);
			}
			else
			{
				// add the root of a variable expression as dependency into the address list
				parser.AddVariableAddress(name);

				parser.Emit(Instruction::DynamicVariable, Variant());
			}
		}
		else
			parser.Variable(name);
	}

	static void Add(DataParser& parser)
	{
		parser.Match('+');
		parser.Push();
		Term(parser);
		parser.Pop(Register::L);
		parser.Emit(Instruction::Add);
	}
	static void Subtract(DataParser& parser)
	{
		parser.Match('-');
		parser.Push();
		Term(parser);
		parser.Pop(Register::L);
		parser.Emit(Instruction::Subtract);
	}
	static void Multiply(DataParser& parser)
	{
		parser.Match('*');
		parser.Push();
		Factor(parser);
		parser.Pop(Register::L);
		parser.Emit(Instruction::Multiply);
	}
	static void Divide(DataParser& parser)
	{
		parser.Match('/');
		parser.Push();
		Factor(parser);
		parser.Pop(Register::L);
		parser.Emit(Instruction::Divide);
	}

	static void Not(DataParser& parser)
	{
		parser.Match('!');
		Factor(parser);
		parser.Emit(Instruction::Not);
	}
	static void Or(DataParser& parser)
	{
		// We already skipped the first '|' during expression
		parser.Match('|');
		parser.Push();
		Relational(parser);
		parser.Pop(Register::L);
		parser.Emit(Instruction::Or);
	}
	static void And(DataParser& parser)
	{
		parser.Match('&', false);
		parser.Match('&');
		parser.Push();
		Relational(parser);
		parser.Pop(Register::L);
		parser.Emit(Instruction::And);
	}
	static void Less(DataParser& parser)
	{
		Instruction instruction = Instruction::Less;
		parser.Match('<', false);
		if (parser.Look() == '=')
		{
			parser.Match('=');
			instruction = Instruction::LessEq;
		}
		else
		{
			parser.SkipWhitespace();
		}
		parser.Push();
		Additive(parser);
		parser.Pop(Register::L);
		parser.Emit(instruction);
	}
	static void Greater(DataParser& parser)
	{
		Instruction instruction = Instruction::Greater;
		parser.Match('>', false);
		if (parser.Look() == '=')
		{
			parser.Match('=');
			instruction = Instruction::GreaterEq;
		}
		else
		{
			parser.SkipWhitespace();
		}
		parser.Push();
		Additive(parser);
		parser.Pop(Register::L);
		parser.Emit(instruction);
	}
	static void Equal(DataParser& parser)
	{
		parser.Match('=', false);
		parser.Match('=');
		parser.Push();
		Additive(parser);
		parser.Pop(Register::L);
		parser.Emit(Instruction::Equal);
	}
	static void NotEqual(DataParser& parser)
	{
		parser.Match('!', false);
		parser.Match('=');
		parser.Push();
		Additive(parser);
		parser.Pop(Register::L);
		parser.Emit(Instruction::NotEqual);
	}

	static void Ternary(DataParser& parser)
	{
		size_t jump_false_branch = parser.InstructionIndex();
		parser.Emit(Instruction::JumpIfZero);

		parser.Match('?');
		Expression(parser);
		size_t jump_end = parser.InstructionIndex();
		parser.Emit(Instruction::Jump);

		parser.Match(':');
		size_t false_branch = parser.InstructionIndex();
		Expression(parser);

		size_t end = parser.InstructionIndex();
		parser.PatchInstruction(jump_false_branch, InstructionData{Instruction::JumpIfZero, Variant((uint64_t)false_branch)});
		parser.PatchInstruction(jump_end, InstructionData{Instruction::Jump, Variant((uint64_t)end)});
	}

	static void Function(DataParser& parser, Instruction function_type, String&& func_name, bool first_argument_piped)
	{
		RMLUI_ASSERT(function_type == Instruction::TransformFnc || function_type == Instruction::EventFnc);

		// We already matched the variable name, and also pushed the first argument to the stack if it was piped using '|'.
		int num_arguments = first_argument_piped ? 1 : 0;
		if (parser.Look() == '(')
		{
			bool looping = true;

			parser.Match('(');
			if (parser.Look() == ')')
			{
				parser.Match(')');
				looping = false;
			}

			while (looping)
			{
				num_arguments += 1;
				Expression(parser);
				parser.Push();

				switch (parser.Look())
				{
				case ')':
					parser.Match(')');
					looping = false;
					break;
				case ',': parser.Match(','); break;
				default:
					parser.Expected("one of ')' or ','");
					looping = false;
					break;
				}
			}
		}
		else
		{
			parser.SkipWhitespace();
		}

		parser.Function(function_type, num_arguments, std::move(func_name));
	}

} // namespace Parse

static String DumpProgram(const Program& program)
{
	String str;
	for (size_t i = 0; i < program.size(); i++)
	{
		String instruction_str = program[i].data.Get<String>();
		str += CreateString("  %4zu  '%c'  %s\n", i, char(program[i].instruction), instruction_str.c_str());
	}
	return str;
}

class DataInterpreter {
public:
	DataInterpreter(const Program& program, const AddressList& addresses, DataExpressionInterface expression_interface, bool silent = false) :
		program(program), addresses(addresses), expression_interface(expression_interface), silent(silent)
	{}

	bool Error(const String& message) const
	{
		if (silent)
			return false;
		Log::Message(Log::LT_WARNING, "Error during execution. %s", message.c_str());
		RMLUI_ERROR;
		return false;
	}

	bool Run()
	{
		bool success = true;
		size_t i = 0;
		while (i < program.size())
		{
			size_t next_instruction = i + 1;
			if (!Execute(program[i].instruction, program[i].data, next_instruction))
			{
				success = false;
				break;
			}
			i = next_instruction;
		}

		if (silent)
			return success;

		if (success && !stack.empty())
			Log::Message(Log::LT_WARNING, "Possible data interpreter stack corruption. Stack size is %zu at end of execution (should be zero).",
				stack.size());

		if (!success)
		{
			String program_str = DumpProgram(program);
			Log::Message(Log::LT_WARNING, "Failed to execute program with %zu instructions:", program.size());
			Log::Message(Log::LT_WARNING, "%s", program_str.c_str());
		}

		return success;
	}

	Variant Result() const { return R; }

private:
	Variant R, L;
	Vector<Variant> stack;

	const Program& program;
	const AddressList& addresses;
	DataExpressionInterface expression_interface;
	bool silent = false;

	bool Execute(const Instruction instruction, const Variant& data, size_t& next_instruction)
	{
		auto AnyString = [](const Variant& v1, const Variant& v2) { return v1.GetType() == Variant::STRING || v2.GetType() == Variant::STRING; };

		switch (instruction)
		{
		case Instruction::Push:
		{
			stack.push_back(std::move(R));
			R.Clear();
		}
		break;
		case Instruction::Pop:
		{
			if (stack.empty())
				return Error("Cannot pop stack, it is empty.");

			Register reg = Register(data.Get<int>(-1));
			switch (reg)
			{
				// clang-format off
			case Register::R:  R = stack.back(); stack.pop_back(); break;
			case Register::L:  L = stack.back(); stack.pop_back(); break;
				// clang-format on
			default: return Error(CreateString("Invalid register %d.", int(reg)));
			}
		}
		break;
		case Instruction::Literal:
		{
			R = data;
		}
		break;
		case Instruction::DynamicVariable:
		{
			auto str = R.Get<String>();
			auto address = expression_interface.ParseAddress(str);
			if (address.empty())
				return Error("Variable address not found.");
			R = expression_interface.GetValue(address);
		}
		break;
		case Instruction::Variable:
		{
			size_t variable_index = size_t(data.Get<int>(-1));
			if (variable_index < addresses.size())
				R = expression_interface.GetValue(addresses[variable_index]);
			else
				return Error("Variable address not found.");
		}
		break;
		case Instruction::Add:
		{
			if (AnyString(L, R))
				R = Variant(L.Get<String>() + R.Get<String>());
			else
				R = Variant(L.Get<double>() + R.Get<double>());
		}
		break;
			// clang-format off
		case Instruction::Subtract:  R = Variant(L.Get<double>() - R.Get<double>());  break;
		case Instruction::Multiply:  R = Variant(L.Get<double>() * R.Get<double>());  break;
		case Instruction::Divide:    R = Variant(L.Get<double>() / R.Get<double>());  break;
		case Instruction::Not:       R = Variant(!R.Get<bool>());                     break;
		case Instruction::And:       R = Variant(L.Get<bool>() && R.Get<bool>());     break;
		case Instruction::Or:        R = Variant(L.Get<bool>() || R.Get<bool>());     break;
		case Instruction::Less:      R = Variant(L.Get<double>() < R.Get<double>());  break;
		case Instruction::LessEq:    R = Variant(L.Get<double>() <= R.Get<double>()); break;
		case Instruction::Greater:   R = Variant(L.Get<double>() > R.Get<double>());  break;
		case Instruction::GreaterEq: R = Variant(L.Get<double>() >= R.Get<double>()); break;
			// clang-format on
		case Instruction::Equal:
		{
			if (AnyString(L, R))
				R = Variant(L.Get<String>() == R.Get<String>());
			else
				R = Variant(L.Get<double>() == R.Get<double>());
		}
		break;
		case Instruction::NotEqual:
		{
			if (AnyString(L, R))
				R = Variant(L.Get<String>() != R.Get<String>());
			else
				R = Variant(L.Get<double>() != R.Get<double>());
		}
		break;
		case Instruction::NumArguments:
		{
			const int num_arguments = data.Get<int>(-1);
			R = num_arguments;
		}
		break;
		case Instruction::TransformFnc:
		case Instruction::EventFnc:
		{
			Vector<Variant> arguments;
			if (!ExtractArgumentsFromStack(arguments))
				return false;

			const String function_name = data.Get<String>();
			const bool result = (instruction == Instruction::TransformFnc ? expression_interface.CallTransform(function_name, arguments, R)
																		  : expression_interface.EventCallback(function_name, arguments));
			if (!result)
			{
				String arguments_str;
				for (size_t i = 0; i < arguments.size(); i++)
				{
					arguments_str += arguments[i].Get<String>();
					if (i < arguments.size() - 1)
						arguments_str += ", ";
				}
				return Error(
					CreateString("Failed to execute %s: %s(%s)", instruction == Instruction::TransformFnc ? "transform function" : "event callback",
						function_name.c_str(), arguments_str.c_str()));
			}
		}
		break;
		case Instruction::Assign:
		{
			size_t variable_index = size_t(data.Get<int>(-1));
			if (variable_index < addresses.size())
			{
				if (!expression_interface.SetValue(addresses[variable_index], R))
					return Error("Could not assign to variable.");
			}
			else
				return Error("Variable address not found.");
		}
		break;
		case Instruction::CastToInt:
		{
			int tmp;
			if (!R.GetInto(tmp))
				return Error("Could not cast value to int.");
			else
				R = tmp;
		}
		break;
		case Instruction::JumpIfZero:
		{
			if (!R.Get<bool>())
				next_instruction = data.Get<size_t>(0);
		}
		break;
		case Instruction::Jump:
		{
			next_instruction = data.Get<size_t>(0);
		}
		break;
		default: RMLUI_ERRORMSG("Instruction not implemented."); break;
		}
		return true;
	}

	bool ExtractArgumentsFromStack(Vector<Variant>& out_arguments)
	{
		int num_arguments = R.Get<int>(-1);
		if (num_arguments < 0)
			return Error("Invalid number of arguments.");
		if (stack.size() < size_t(num_arguments))
			return Error(CreateString("Cannot pop %d arguments, stack contains only %zu elements.", num_arguments, stack.size()));

		const auto it_stack_begin_arguments = stack.end() - num_arguments;
		out_arguments.insert(out_arguments.end(), std::make_move_iterator(it_stack_begin_arguments), std::make_move_iterator(stack.end()));

		stack.erase(it_stack_begin_arguments, stack.end());
		return true;
	}
};

DataExpression::DataExpression() {}

DataExpression::~DataExpression() {}

bool DataExpression::Parse(const String& expression, const DataExpressionInterface& expression_interface, bool is_assignment_expression)
{
	DataParseMemo* memo = DataParseMemo::GetActive();
	if (memo)
	{
		const DataParseMemo::CompiledProgram* compiled = memo->FindProgram(expression, is_assignment_expression);
		RMLUI_ARC_PERF_CODE(if (!compiled && memo->IsSeeded()) RMLUI_ARC_PERF_COUNT(DataExpressionSeedMisses, 1);)
		if (compiled)
		{
			// The shared program indexes addresses by slot, so it is exact only when every slot resolves for this element.
			AddressList resolved_addresses;
			resolved_addresses.reserve(compiled->address_names.size());
			for (const String& address_name : compiled->address_names)
			{
				DataAddress address = expression_interface.ParseAddress(address_name, false);
				if (address.empty())
					break;
				resolved_addresses.push_back(std::move(address));
			}
			if (resolved_addresses.size() == compiled->address_names.size())
			{
				program = compiled->program;
				addresses = std::move(resolved_addresses);
				return true;
			}
		}
	}

	DataParser parser(expression, expression_interface);
	if (memo)
		parser.SetRecordAddressNames();
	if (!parser.Parse(is_assignment_expression))
		return false;

	program = MakeShared<const Program>(parser.ReleaseProgram());
	addresses = parser.ReleaseAddresses();

	if (memo && parser.AllAddressesResolved())
		memo->StoreProgram(expression, is_assignment_expression, DataParseMemo::CompiledProgram{program, parser.ReleaseAddressNames()});

	return true;
}

bool DataExpression::Run(const DataExpressionInterface& expression_interface, Variant& out_value)
{
	RMLUI_ASSERT(program);
	DataInterpreter interpreter(*program, addresses, expression_interface);

	if (!interpreter.Run())
		return false;

	out_value = interpreter.Result();
	return true;
}

static DataParseMemo* active_data_parse_memo = nullptr;

DataParseMemo::DataParseMemo() : previous(active_data_parse_memo)
{
	active_data_parse_memo = this;
}

DataParseMemo::~DataParseMemo()
{
	RMLUI_ASSERT(active_data_parse_memo == this);
	active_data_parse_memo = previous;
}

DataParseMemo* DataParseMemo::GetActive()
{
	return active_data_parse_memo;
}

const DataParseMemo::CompiledProgram* DataParseMemo::FindProgram(const String& expression, bool is_assignment_expression) const
{
	const UnorderedMap<String, CompiledProgram>& map = programs[is_assignment_expression ? 1 : 0];
	const UnorderedMap<String, CompiledProgram>::const_iterator it = map.find(expression);
	return it == map.end() ? nullptr : &it->second;
}

void DataParseMemo::StoreProgram(const String& expression, bool is_assignment_expression, CompiledProgram compiled)
{
	programs[is_assignment_expression ? 1 : 0].emplace(expression, std::move(compiled));
}

void DataParseMemo::SeedProgram(const String& expression, bool is_assignment_expression, CompiledProgram compiled)
{
	seeded = true;
	StoreProgram(expression, is_assignment_expression, std::move(compiled));
}

const DataAddress* DataParseMemo::FindAddressTemplate(const String& address_str) const
{
	const UnorderedMap<String, DataAddress>::const_iterator it = address_templates.find(address_str);
	return it == address_templates.end() ? nullptr : &it->second;
}

const DataAddress& DataParseMemo::StoreAddressTemplate(const String& address_str, DataAddress address)
{
	return address_templates.emplace(address_str, std::move(address)).first->second;
}

DataExpressionInterface::DataExpressionInterface(DataModel* data_model, Element* element, Event* event) :
	data_model(data_model), element(element), event(event)
{}

DataExpressionInterface::DataExpressionInterface(TransformFuncRegister* transform_register) : transform_register(transform_register) {}

DataAddress DataExpressionInterface::ParseAddress(const String& address_str, bool log_missing) const
{
	if (address_str.size() >= 4 && address_str[0] == 'e' && address_str[1] == 'v' && address_str[2] == '.')
		return DataAddress{DataAddressEntry("ev"), DataAddressEntry(address_str.substr(3))};

	return data_model ? data_model->ResolveAddress(address_str, element, log_missing) : DataAddress();
}
Variant DataExpressionInterface::GetValue(const DataAddress& address) const
{
	Variant result;
	if (event && address.size() == 2 && address.front().name == "ev")
	{
		auto& parameters = event->GetParameters();
		auto it = parameters.find(address.back().name);
		if (it != parameters.end())
			result = it->second;
	}
	else if (data_model)
	{
		data_model->GetVariableInto(address, result);
	}
	return result;
}

bool DataExpressionInterface::SetValue(const DataAddress& address, const Variant& value) const
{
	bool result = false;
	if (data_model && !address.empty())
	{
		if (DataVariable variable = data_model->GetVariable(address))
			result = variable.Set(value);

		if (result)
			data_model->DirtyVariable(address.front().name);
	}
	return result;
}

bool DataExpressionInterface::CallTransform(const String& name, const VariantList& arguments, Variant& out_result)
{
	if (data_model)
		return data_model->CallTransform(name, arguments, out_result);
	return transform_register ? transform_register->Call(name, arguments, out_result) : false;
}

bool DataExpressionInterface::EventCallback(const String& name, const VariantList& arguments)
{
	if (!data_model || !event)
		return false;

	const DataEventFunc* func = data_model->GetEventCallback(name);
	if (!func || !*func)
		return false;

	DataModelHandle handle(data_model);
	func->operator()(handle, *event, arguments);
	return true;
}

ConstantDataExpressionResult EvaluateConstantDataExpression(const String& expression, DataTypeRegister& type_register, Variant& out_value)
{
	DataParser parser(expression, DataExpressionInterface());
	parser.SetConstantOnly();
	if (!parser.Parse(false))
		return ConstantDataExpressionResult::Invalid;
	if (parser.ReferencesVariable())
		return ConstantDataExpressionResult::ReferencesVariable;

	const Program program = parser.ReleaseProgram();
	const AddressList addresses = parser.ReleaseAddresses();
	DataInterpreter interpreter(program, addresses, DataExpressionInterface(type_register.GetTransformFuncRegister()), true);
	if (!interpreter.Run())
		return ConstantDataExpressionResult::Invalid;

	out_value = interpreter.Result();
	return ConstantDataExpressionResult::Constant;
}

bool ParseDataExpressionModelFree(const String& expression, bool is_assignment_expression, DataParseMemo::CompiledProgram& out_compiled)
{
	DataParser parser(expression, DataExpressionInterface());
	parser.SetModelFree();
	if (!parser.Parse(is_assignment_expression))
		return false;

	out_compiled.program = MakeShared<const Program>(parser.ReleaseProgram());
	out_compiled.address_names = parser.ReleaseAddressNames();
	return true;
}

namespace DataExpressionBinary {

	template <typename T>
	void Append(Vector<byte>& out, const T& value)
	{
		const byte* bytes = reinterpret_cast<const byte*>(&value);
		out.insert(out.end(), bytes, bytes + sizeof(T));
	}

	template <typename T>
	bool Take(const byte*& cursor, const byte* end, T& out_value)
	{
		if (size_t(end - cursor) < sizeof(T))
			return false;
		memcpy(&out_value, cursor, sizeof(T));
		cursor += sizeof(T);
		return true;
	}

	bool IsInstruction(int32_t value)
	{
		switch (Instruction(value))
		{
		case Instruction::Push:
		case Instruction::Pop:
		case Instruction::Literal:
		case Instruction::Variable:
		case Instruction::Add:
		case Instruction::Subtract:
		case Instruction::Multiply:
		case Instruction::Divide:
		case Instruction::Not:
		case Instruction::And:
		case Instruction::Or:
		case Instruction::Less:
		case Instruction::LessEq:
		case Instruction::Greater:
		case Instruction::GreaterEq:
		case Instruction::Equal:
		case Instruction::NotEqual:
		case Instruction::NumArguments:
		case Instruction::TransformFnc:
		case Instruction::EventFnc:
		case Instruction::Assign:
		case Instruction::DynamicVariable:
		case Instruction::CastToInt:
		case Instruction::Jump:
		case Instruction::JumpIfZero: return true;
		}
		return false;
	}

} // namespace DataExpressionBinary

bool EncodeDataProgram(const Program& program, Vector<byte>& out_bytes)
{
	using namespace DataExpressionBinary;
	Append(out_bytes, uint32_t(program.size()));
	for (const InstructionData& instruction : program)
	{
		const Variant& data = instruction.data;
		const Variant::Type type = data.GetType();
		Append(out_bytes, int32_t(instruction.instruction));
		Append(out_bytes, uint8_t(type));

		if (type == Variant::NONE)
			continue;
		if (type == Variant::BOOL)
			Append(out_bytes, uint8_t(data.Get<bool>() ? 1 : 0));
		else if (type == Variant::INT)
			Append(out_bytes, int32_t(data.Get<int>()));
		else if (type == Variant::DOUBLE)
			Append(out_bytes, data.Get<double>());
		else if (type == Variant::UINT64)
			Append(out_bytes, data.Get<uint64_t>());
		else if (type == Variant::STRING)
		{
			const String& value = data.GetReference<String>();
			const byte* bytes = reinterpret_cast<const byte*>(value.data());
			Append(out_bytes, uint32_t(value.size()));
			out_bytes.insert(out_bytes.end(), bytes, bytes + value.size());
		}
		else
			return false;
	}
	return true;
}

SharedPtr<const Program> DecodeDataProgram(const byte* data, size_t size)
{
	using namespace DataExpressionBinary;
	const byte* cursor = data;
	const byte* end = data + size;

	uint32_t count = 0;
	if (!Take(cursor, end, count) || count > size)
		return nullptr;

	Program program;
	program.reserve(count);
	for (uint32_t i = 0; i < count; i++)
	{
		int32_t instruction = 0;
		uint8_t stored_type = 0;
		if (!Take(cursor, end, instruction) || !IsInstruction(instruction) || !Take(cursor, end, stored_type))
			return nullptr;

		const Variant::Type type = Variant::Type(stored_type);
		Variant value;
		if (type == Variant::BOOL)
		{
			uint8_t flag = 0;
			if (!Take(cursor, end, flag))
				return nullptr;
			value = Variant(flag != 0);
		}
		else if (type == Variant::INT)
		{
			int32_t number = 0;
			if (!Take(cursor, end, number))
				return nullptr;
			value = Variant(int(number));
		}
		else if (type == Variant::DOUBLE)
		{
			double number = 0.0;
			if (!Take(cursor, end, number))
				return nullptr;
			value = Variant(number);
		}
		else if (type == Variant::UINT64)
		{
			uint64_t number = 0;
			if (!Take(cursor, end, number))
				return nullptr;
			value = Variant(number);
		}
		else if (type == Variant::STRING)
		{
			uint32_t length = 0;
			if (!Take(cursor, end, length) || size_t(end - cursor) < length)
				return nullptr;
			value = Variant(String(reinterpret_cast<const char*>(cursor), length));
			cursor += length;
		}
		else if (type != Variant::NONE)
			return nullptr;

		program.push_back(InstructionData{Instruction(instruction), std::move(value)});
	}

	if (cursor != end)
		return nullptr;

	return MakeShared<const Program>(std::move(program));
}

const char* SplitDataText(const String& in_text, String& out_text, Vector<DataTextExpression>& out_expressions)
{
	out_text.reserve(in_text.size());

	size_t begin_brackets = 0;
	size_t cur = 0;
	char previous = 0;
	bool was_in_brackets = false;
	bool in_brackets = false;
	bool in_string = false;

	for (char c : in_text)
	{
		was_in_brackets = in_brackets;

		if (const char* error_str = XMLParseTools::ParseDataBrackets(in_brackets, in_string, c, previous))
			return error_str;

		if (!was_in_brackets && in_brackets)
		{
			begin_brackets = cur;
		}
		else if (was_in_brackets && !in_brackets)
		{
			out_expressions.push_back(DataTextExpression{out_text.size(), String(in_text.begin() + begin_brackets + 1, in_text.begin() + cur - 1)});

			// Reset char so that it won't be appended to the output
			c = 0;
		}
		else if (!in_brackets && previous)
		{
			out_text.push_back(previous);
		}

		cur++;
		previous = c;
	}

	if (!in_brackets && previous)
		out_text.push_back(previous);

	return nullptr;
}

namespace DataExpressionTable {

	struct TypeEntry {
		const char* type_name;
		DataExpressionSite view;
		DataExpressionSite controller;
	};

	static const TypeEntry type_entries[] = {
		{"attr", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"attrif", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"class", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"if", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"visible", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"rml", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"style", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"text", DataExpressionSite::ViewText, DataExpressionSite::None},
		{"value", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"checked", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"world", DataExpressionSite::ViewCommon, DataExpressionSite::None},
		{"alias", DataExpressionSite::None, DataExpressionSite::None},
		{"for", DataExpressionSite::None, DataExpressionSite::None},
		{"event", DataExpressionSite::None, DataExpressionSite::ControllerEvent},
	};

	bool IsAssignment(DataExpressionSite site)
	{
		return site == DataExpressionSite::ControllerEvent;
	}

	static void VisitAttributeSite(DataExpressionSite site, const String& attribute_value,
		const Function<void(const String& expression, bool is_assignment_expression)>& function)
	{
		if (site == DataExpressionSite::ViewCommon || site == DataExpressionSite::ControllerEvent)
			function(attribute_value, IsAssignment(site));
	}

	void ForEachAttributeExpression(const String& attribute_name, const String& attribute_value,
		const Function<void(const String& expression, bool is_assignment_expression)>& function)
	{
		constexpr size_t data_str_length = sizeof("data-") - 1;
		if (attribute_name.size() <= data_str_length || attribute_name.compare(0, data_str_length, "data-") != 0)
			return;

		const size_t type_end = attribute_name.find('-', data_str_length);
		const size_t type_size = (type_end == String::npos ? String::npos : type_end - data_str_length);
		const String type_name = attribute_name.substr(data_str_length, type_size);

		for (const TypeEntry& entry : type_entries)
		{
			if (type_name == entry.type_name)
			{
				VisitAttributeSite(entry.view, attribute_value, function);
				VisitAttributeSite(entry.controller, attribute_value, function);
				return;
			}
		}
	}

	void ForEachTextExpression(const String& element_text, const Function<void(const String& expression, bool is_assignment_expression)>& function)
	{
		String text;
		Vector<DataTextExpression> expressions;
		if (SplitDataText(element_text, text, expressions))
			return;

		for (const DataTextExpression& expression : expressions)
			function(expression.expression, IsAssignment(DataExpressionSite::ViewText));
	}

} // namespace DataExpressionTable

} // namespace Rml
