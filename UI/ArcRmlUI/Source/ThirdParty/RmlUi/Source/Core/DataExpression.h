#pragma once

#include "../../Include/RmlUi/Core/DataTypes.h"
#include "../../Include/RmlUi/Core/Header.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Element;
class DataModel;
struct InstructionData;
using Program = Vector<InstructionData>;
using AddressList = Vector<DataAddress>;

class DataExpressionInterface {
public:
	DataExpressionInterface() = default;
	DataExpressionInterface(DataModel* data_model, Element* element, Event* event = nullptr);
	explicit DataExpressionInterface(TransformFuncRegister* transform_register);

	DataAddress ParseAddress(const String& address_str, bool log_missing = true) const;
	Variant GetValue(const DataAddress& address) const;
	bool SetValue(const DataAddress& address, const Variant& value) const;
	bool CallTransform(const String& name, const VariantList& arguments, Variant& out_result);
	bool EventCallback(const String& name, const VariantList& arguments);

private:
	DataModel* data_model = nullptr;
	Element* element = nullptr;
	Event* event = nullptr;
	TransformFuncRegister* transform_register = nullptr;
};

class DataExpression {
public:
	DataExpression();
	~DataExpression();

	bool Parse(const String& expression, const DataExpressionInterface& expression_interface, bool is_assignment_expression);

	bool Run(const DataExpressionInterface& expression_interface, Variant& out_value);

	// Available after Parse()
	const AddressList& GetAddresses() const { return addresses; }

private:
	SharedPtr<const Program> program;
	AddressList addresses;
};

// Memo of the data expressions parsed while it is installed (one document load); a nested memo restores the enclosing one.
class DataParseMemo {
public:
	struct CompiledProgram {
		SharedPtr<const Program> program;
		StringList address_names;
	};

	DataParseMemo();
	~DataParseMemo();
	DataParseMemo(const DataParseMemo&) = delete;
	DataParseMemo& operator=(const DataParseMemo&) = delete;

	static DataParseMemo* GetActive();

	const CompiledProgram* FindProgram(const String& expression, bool is_assignment_expression) const;
	void StoreProgram(const String& expression, bool is_assignment_expression, CompiledProgram compiled);
	void SeedProgram(const String& expression, bool is_assignment_expression, CompiledProgram compiled);
	bool IsSeeded() const { return seeded; }

	const DataAddress* FindAddressTemplate(const String& address_str) const;
	const DataAddress& StoreAddressTemplate(const String& address_str, DataAddress address);

private:
	UnorderedMap<String, CompiledProgram> programs[2];
	UnorderedMap<String, DataAddress> address_templates;
	DataParseMemo* previous;
	bool seeded = false;
};

bool ParseDataExpressionModelFree(const String& expression, bool is_assignment_expression, DataParseMemo::CompiledProgram& out_compiled);

bool EncodeDataProgram(const Program& program, Vector<byte>& out_bytes);
SharedPtr<const Program> DecodeDataProgram(const byte* data, size_t size);

struct DataTextExpression {
	size_t index;
	String expression;
};

const char* SplitDataText(const String& in_text, String& out_text, Vector<DataTextExpression>& out_expressions);

enum class DataExpressionSite : uint8_t { None, ViewCommon, ViewText, ControllerEvent };

namespace DataExpressionTable {
	bool IsAssignment(DataExpressionSite site);

	void ForEachAttributeExpression(const String& attribute_name, const String& attribute_value,
		const Function<void(const String& expression, bool is_assignment_expression)>& function);
	void ForEachTextExpression(const String& element_text, const Function<void(const String& expression, bool is_assignment_expression)>& function);
} // namespace DataExpressionTable

} // namespace Rml
