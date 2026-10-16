// Recursive-descent grammar after Ladybird (https://github.com/LadybirdBrowser/ladybird) Libraries/LibWeb/Rust/src/css/parser/calc_parser.rs.
// Ladybird is BSD-2-Clause licensed, see LICENSE-ladybird.txt.
#include "PropertyParserCalc.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "CalcExpression.h"
#include <stdlib.h>

namespace Rml {

namespace {

	struct CalcParserToken {
		enum class Type { Number, Function, OpenParen, CloseParen, Comma, Plus, Minus, Star, Slash };
		Type type;
		NumericValue value;
		String name;
	};

	enum class CalcValueType { Invalid, Number, Length };

	constexpr int CalcMaxNesting = 32;

	bool IsCalcDigit(char c)
	{
		return c >= '0' && c <= '9';
	}

	bool IsCalcLetter(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	bool ParseCalcUnit(const String& name, Unit& out_unit)
	{
		static const struct {
			const char* name;
			Unit unit;
		} units[] = {
			{"", Unit::NUMBER},
			{"%", Unit::PERCENT},
			{"px", Unit::PX},
			{"dp", Unit::DP},
			{"vw", Unit::VW},
			{"vh", Unit::VH},
			{"em", Unit::EM},
			{"rem", Unit::REM},
			{"in", Unit::INCH},
			{"cm", Unit::CM},
			{"mm", Unit::MM},
			{"pt", Unit::PT},
			{"pc", Unit::PC},
			{"deg", Unit::DEG},
			{"rad", Unit::RAD},
		};
		for (const auto& entry : units)
		{
			if (name == entry.name)
			{
				out_unit = entry.unit;
				return true;
			}
		}
		return false;
	}

	bool TokenizeCalc(const String& value, Vector<CalcParserToken>& tokens)
	{
		const size_t length = value.size();
		size_t i = 0;
		while (i < length)
		{
			const char c = value[i];
			if (StringUtilities::IsWhitespace(c))
			{
				i++;
				continue;
			}

			const bool starts_number = IsCalcDigit(c) || (c == '.' && i + 1 < length && IsCalcDigit(value[i + 1])) ||
				((c == '+' || c == '-') && i + 1 < length &&
					(IsCalcDigit(value[i + 1]) || (value[i + 1] == '.' && i + 2 < length && IsCalcDigit(value[i + 2]))));

			if (starts_number)
			{
				const size_t start = i;
				if (c == '+' || c == '-')
					i++;
				while (i < length && IsCalcDigit(value[i]))
					i++;
				if (i + 1 < length && value[i] == '.' && IsCalcDigit(value[i + 1]))
				{
					i++;
					while (i < length && IsCalcDigit(value[i]))
						i++;
				}
				if (i + 1 < length && (value[i] == 'e' || value[i] == 'E'))
				{
					size_t exponent = i + 1;
					if (exponent < length && (value[exponent] == '+' || value[exponent] == '-'))
						exponent++;
					if (exponent < length && IsCalcDigit(value[exponent]))
					{
						i = exponent;
						while (i < length && IsCalcDigit(value[i]))
							i++;
					}
				}
				const float number = strtof(value.substr(start, i - start).c_str(), nullptr);

				const size_t unit_start = i;
				if (i < length && value[i] == '%')
					i++;
				else
					while (i < length && IsCalcLetter(value[i]))
						i++;

				if (i < length && (IsCalcLetter(value[i]) || IsCalcDigit(value[i]) || value[i] == '-' || value[i] == '_'))
					return false;

				Unit unit = Unit::UNKNOWN;
				if (!ParseCalcUnit(StringUtilities::ToLower(value.substr(unit_start, i - unit_start)), unit))
					return false;

				tokens.push_back(CalcParserToken{CalcParserToken::Type::Number, NumericValue(number, unit), String()});
				continue;
			}

			if (IsCalcLetter(c))
			{
				const size_t start = i;
				while (i < length && (IsCalcLetter(value[i]) || value[i] == '-'))
					i++;
				if (i >= length || value[i] != '(')
					return false;
				tokens.push_back(CalcParserToken{CalcParserToken::Type::Function, NumericValue(), StringUtilities::ToLower(value.substr(start, i - start))});
				i++;
				continue;
			}

			CalcParserToken::Type type;
			switch (c)
			{
			case '(': type = CalcParserToken::Type::OpenParen; break;
			case ')': type = CalcParserToken::Type::CloseParen; break;
			case ',': type = CalcParserToken::Type::Comma; break;
			case '+': type = CalcParserToken::Type::Plus; break;
			case '-': type = CalcParserToken::Type::Minus; break;
			case '*': type = CalcParserToken::Type::Star; break;
			case '/': type = CalcParserToken::Type::Slash; break;
			default: return false;
			}
			tokens.push_back(CalcParserToken{type, NumericValue(), String()});
			i++;
		}
		return true;
	}

	class CalcParser {
	public:
		CalcParser(const Vector<CalcParserToken>& tokens, Units units) : tokens(tokens), units(units) {}

		bool ParseRoot(Vector<CalcNode>& out_nodes, CalcValueType root_type)
		{
			const CalcParserToken* first = Peek();
			if (!first || first->type != CalcParserToken::Type::Function)
				return false;

			const CalcValueType type = ParseValue();
			if (type != root_type || index != tokens.size() || nodes.size() > CalcExpression::MaxNodes)
				return false;

			out_nodes = std::move(nodes);
			return true;
		}

	private:
		const CalcParserToken* Peek() const { return index < tokens.size() ? &tokens[index] : nullptr; }

		bool Accept(CalcParserToken::Type type)
		{
			if (index >= tokens.size() || tokens[index].type != type)
				return false;
			index++;
			return true;
		}

		bool Emit(CalcNode::Type type, uint16_t argument_count = 0, NumericValue value = NumericValue())
		{
			if (nodes.size() >= CalcExpression::MaxNodes)
				return false;
			nodes.push_back(CalcNode{type, argument_count, value});
			return true;
		}

		CalcValueType ParseSum()
		{
			CalcValueType type = ParseProduct();
			while (type != CalcValueType::Invalid)
			{
				const CalcParserToken* token = Peek();
				if (!token || (token->type != CalcParserToken::Type::Plus && token->type != CalcParserToken::Type::Minus))
					break;
				index++;

				const CalcValueType rhs = ParseProduct();
				if (rhs != type || !Emit(token->type == CalcParserToken::Type::Plus ? CalcNode::Type::Add : CalcNode::Type::Subtract))
					return CalcValueType::Invalid;
			}
			return type;
		}

		CalcValueType ParseProduct()
		{
			CalcValueType type = ParseValue();
			while (type != CalcValueType::Invalid)
			{
				const CalcParserToken* token = Peek();
				if (!token || (token->type != CalcParserToken::Type::Star && token->type != CalcParserToken::Type::Slash))
					break;
				index++;

				const CalcValueType rhs = ParseValue();
				if (rhs == CalcValueType::Invalid)
					return CalcValueType::Invalid;

				if (token->type == CalcParserToken::Type::Star)
				{
					if (type == CalcValueType::Length && rhs == CalcValueType::Length)
						return CalcValueType::Invalid;
					if (type == CalcValueType::Number)
						type = rhs;
					if (!Emit(CalcNode::Type::Multiply))
						return CalcValueType::Invalid;
				}
				else
				{
					if (rhs != CalcValueType::Number || !Emit(CalcNode::Type::Divide))
						return CalcValueType::Invalid;
				}
			}
			return type;
		}

		CalcValueType ParseArguments(CalcNode::Type function_type)
		{
			CalcValueType type = ParseSum();
			int count = 1;
			while (type != CalcValueType::Invalid && Accept(CalcParserToken::Type::Comma))
			{
				if (ParseSum() != type)
					return CalcValueType::Invalid;
				count++;
			}
			if (type == CalcValueType::Invalid || !Accept(CalcParserToken::Type::CloseParen))
				return CalcValueType::Invalid;

			if (function_type == CalcNode::Type::Clamp)
			{
				if (count != 3 || !Emit(CalcNode::Type::Clamp))
					return CalcValueType::Invalid;
			}
			else if (count > 1 && !Emit(function_type, uint16_t(count)))
			{
				return CalcValueType::Invalid;
			}
			return type;
		}

		CalcValueType ParseValue()
		{
			const CalcParserToken* token = Peek();
			if (!token || depth >= CalcMaxNesting)
				return CalcValueType::Invalid;
			index++;

			switch (token->type)
			{
			case CalcParserToken::Type::Number:
			{
				const Unit unit = token->value.unit;
				if (unit != Unit::NUMBER && !Any(unit & units))
					return CalcValueType::Invalid;
				if (!Emit(CalcNode::Type::Value, 0, token->value))
					return CalcValueType::Invalid;
				return unit == Unit::NUMBER ? CalcValueType::Number : CalcValueType::Length;
			}
			case CalcParserToken::Type::OpenParen:
			case CalcParserToken::Type::Function:
			{
				CalcNode::Type function_type = CalcNode::Type::Value;
				if (token->type == CalcParserToken::Type::Function)
				{
					if (token->name == "min")
						function_type = CalcNode::Type::Min;
					else if (token->name == "max")
						function_type = CalcNode::Type::Max;
					else if (token->name == "clamp")
						function_type = CalcNode::Type::Clamp;
					else if (token->name != "calc")
						return CalcValueType::Invalid;
				}

				depth++;
				CalcValueType type = CalcValueType::Invalid;
				if (function_type == CalcNode::Type::Value)
				{
					type = ParseSum();
					if (!Accept(CalcParserToken::Type::CloseParen))
						type = CalcValueType::Invalid;
				}
				else
				{
					type = ParseArguments(function_type);
				}
				depth--;
				return type;
			}
			default: break;
			}
			return CalcValueType::Invalid;
		}

		const Vector<CalcParserToken>& tokens;
		Units units;
		size_t index = 0;
		int depth = 0;
		Vector<CalcNode> nodes;
	};

} // namespace

PropertyParserCalc::PropertyParserCalc(Units units, bool non_negative) : units(units), non_negative(non_negative) {}

PropertyParserCalc::~PropertyParserCalc() {}

bool PropertyParserCalc::ParseValue(Property& property, const String& value, const ParameterMap& /*parameters*/) const
{
	return ParseMathFunction(property, value, units, non_negative);
}

bool PropertyParserCalc::ParseMathFunction(Property& property, const String& value, Units units, bool non_negative)
{
	Vector<CalcParserToken> tokens;
	if (!TokenizeCalc(value, tokens))
		return false;

	Vector<CalcNode> nodes;
	CalcParser parser(tokens, units);
	if (!parser.ParseRoot(nodes, units == Unit::NUMBER ? CalcValueType::Number : CalcValueType::Length))
		return false;

	CalcExpressionPtr expression = MakeShared<const CalcExpression>(std::move(nodes), non_negative);

	NumericValue folded;
	if (FoldCalcExpression(*expression, folded))
	{
		property.value = Variant(non_negative ? Math::Max(folded.number, 0.f) : folded.number);
		property.unit = folded.unit;
		return true;
	}

	property.value = Variant(std::move(expression));
	property.unit = Unit::CALC;
	return true;
}

} // namespace Rml
