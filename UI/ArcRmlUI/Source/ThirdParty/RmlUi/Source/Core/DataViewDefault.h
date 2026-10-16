#pragma once

#include "../../Include/RmlUi/Core/EventListener.h"
#include "../../Include/RmlUi/Core/Header.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "../../Include/RmlUi/Core/Types.h"
#include "../../Include/RmlUi/Core/Variant.h"
#include "../../Include/RmlUi/Core/WorldAnchors.h"
#include "DataView.h"

namespace Rml {

class Element;
class WorldAnchorSet;
class DataExpression;
class DataVariable;
using DataExpressionPtr = UniquePtr<DataExpression>;

class DataViewCommon : public DataView {
public:
	DataViewCommon(Element* element, String override_modifier = String(), int sort_offset = 0);

	bool Initialize(DataModel& model, Element* element, const String& expression, const String& modifier) override;

	void ForEachVariableAddress(const Function<void(const DataAddress&)>& function) const override;

protected:
	const String& GetModifier() const;
	DataExpression& GetExpression();

	void Release() override;

private:
	String modifier;
	DataExpressionPtr expression;
};

class DataViewAttribute : public DataViewCommon {
public:
	DataViewAttribute(Element* element);
	DataViewAttribute(Element* element, String override_attribute, int sort_offset);

	bool Update(DataModel& model) override;
};

class DataViewAttributeIf final : public DataViewCommon {
public:
	DataViewAttributeIf(Element* element);

	bool Update(DataModel& model) override;
};

class DataViewValue final : public DataViewAttribute {
public:
	DataViewValue(Element* element);
};

class DataViewChecked final : public DataViewCommon {
public:
	DataViewChecked(Element* element);

	bool Update(DataModel& model) override;
};

class DataViewStyle final : public DataViewCommon {
public:
	DataViewStyle(Element* element);

	bool Update(DataModel& model) override;
};

class DataViewClass final : public DataViewCommon {
public:
	DataViewClass(Element* element);

	bool Update(DataModel& model) override;
};

class DataViewRml final : public DataViewCommon {
public:
	DataViewRml(Element* element);

	bool Update(DataModel& model) override;

private:
	String previous_rml;
};

class DataViewIf final : public DataViewCommon {
public:
	DataViewIf(Element* element);

	bool Initialize(DataModel& model, Element* element, const String& expression, const String& modifier) override;

	bool Update(DataModel& model) override;

private:
	ElementAttributes attributes;
	ObserverPtr<Element> branch;
};

class DataViewVisible final : public DataViewCommon {
public:
	DataViewVisible(Element* element);

	bool Update(DataModel& model) override;
};

class DataViewWorld final : public DataViewCommon {
public:
	DataViewWorld(Element* element);

	bool Initialize(DataModel& model, Element* element, const String& expression, const String& modifier) override;

	bool Update(DataModel& model) override;

	bool Place();

protected:
	void Release() override;

private:
	WorldAnchorSet* anchor_set = nullptr;
	WorldPoint world_point;
	bool has_world_point = false;
	bool parse_error_logged = false;
	bool applied_visible = true;
	bool has_applied_translate = false;
	Vector2f applied_translate;
};

class DataViewText final : public DataView {
public:
	DataViewText(Element* in_element);

	bool Initialize(DataModel& model, Element* element, const String& expression, const String& modifier) override;

	bool Update(DataModel& model) override;
	void ForEachVariableAddress(const Function<void(const DataAddress&)>& function) const override;

	void RestoreElementSource() override;

protected:
	void Release() override;

private:
	String BuildText() const;

	struct DataEntry {
		size_t index = 0; // Index into 'text'
		DataExpressionPtr data_expression;
		String value;
	};

	String source;
	String text;
	Vector<DataEntry> data_entries;
};

class DataViewFor final : public DataView, public EventListener {
public:
	DataViewFor(Element* element);

	bool Initialize(DataModel& model, Element* element, const String& expression, const String& modifier) override;

	bool Update(DataModel& model) override;

	void ForEachVariableAddress(const Function<void(const DataAddress&)>& function) const override;

	bool OnLayout(DataModel& model) override;

	void ProcessEvent(Event& event) override;

protected:
	void Release() override;

private:
	struct KeyedRow {
		Element* element = nullptr;
		String key;
		int index = 0;
	};

	enum class KeyedRowContent : uint8_t { Unchecked, Valid, Invalid };

	enum class VirtualState : uint8_t { Off, Unchecked, Valid, Invalid };

	const String* RMLContents() const;

	bool InitializeKey(Element* element, const String& key_text);
	bool KeyedRowContentValid(Element* anchor) const;
	bool UpdateKeyed(DataModel& model, const DataVariable& variable, Element* anchor, Element* end_marker, int begin, int end);
	bool UpdateRows(DataModel& model, Element* anchor, Element* end_marker, int begin, int end);
	void InsertRowAliases(DataModel& model, Element* row, int index) const;
	Element* InstanceRow(DataModel& model, Element* anchor, Element* before, int index) const;

	bool UpdateVirtual(DataModel& model, const DataVariable& variable, Element* anchor);
	bool SetupVirtual(DataModel& model, Element* anchor);
	void ComputeWindow(int size, int& out_begin, int& out_end) const;
	bool UpdateSpacer(Element* spacer, int lines, float& applied_height) const;
	bool RequestWindowUpdate(DataModel& model);
	Element* FirstRow() const;

	DataAddress container_address;
	String iterator_name;
	String iterator_index_name;
	ElementAttributes attributes;

	ElementList elements;
	int window_begin = 0;

	String key_text;
	DataAddress key_path;
	Vector<KeyedRow> keyed_rows;
	KeyedRowContent keyed_row_content = KeyedRowContent::Unchecked;

	VirtualState virtual_state = VirtualState::Off;
	DataModel* virtual_model = nullptr;
	ObserverPtr<Element> scroll_container;
	ObserverPtr<Element> leading_spacer;
	ObserverPtr<Element> trailing_spacer;
	bool measured = false;
	bool window_requested = false;
	int columns = 1;
	float line_size = 0.f;
	float row_gap = 0.f;
	int applied_begin = 0;
	int applied_end = 0;
	int applied_columns = 0;
	float applied_line_size = 0.f;
	float applied_row_gap = 0.f;
	float applied_leading_height = -1.f;
	float applied_trailing_height = -1.f;
};

class DataViewAlias final : public DataView {
public:
	DataViewAlias(Element* element);
	void ForEachVariableAddress(const Function<void(const DataAddress&)>& function) const override;
	bool Update(DataModel& model) override;
	bool Initialize(DataModel& model, Element* element, const String& expression, const String& modifier) override;

protected:
	void Release() override;
};

} // namespace Rml
