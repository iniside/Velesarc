#include "DataViewDefault.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Context.h"
#include "../../Include/RmlUi/Core/Core.h"
#include "../../Include/RmlUi/Core/DataVariable.h"
#include "../../Include/RmlUi/Core/Event.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/Factory.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/SystemInterface.h"
#include "../../Include/RmlUi/Core/Transform.h"
#include "../../Include/RmlUi/Core/Variant.h"
#include "DataExpression.h"
#include "DataModel.h"
#include "WorldAnchorSet.h"
#include "XMLEventStream.h"
#include <cmath>
#include <cstdlib>

namespace Rml {

// Some data views need to offset the update order for proper behavior.
//  'data-value' may need other attributes applied first, e.g. min/max attributes.
static constexpr int SortOffset_DataValue = 100;
//  'data-checked' may need a value attribute already set.
static constexpr int SortOffset_DataChecked = 110;
//  Structural views add and remove the sibling rows and branches whose views share their depth.
static constexpr int SortOffset_Structural = -1000;

DataViewCommon::DataViewCommon(Element* element, String override_modifier, int sort_offset) :
	DataView(element, sort_offset), modifier(std::move(override_modifier))
{}

bool DataViewCommon::Initialize(DataModel& model, Element* element, const String& expression_str, const String& in_modifier)
{
	// The modifier can be overridden in the constructor
	if (modifier.empty())
		modifier = in_modifier;

	expression = MakeUnique<DataExpression>();
	DataExpressionInterface expr_interface(&model, element);

	bool result = expression->Parse(expression_str, expr_interface, DataExpressionTable::IsAssignment(DataExpressionSite::ViewCommon));
	return result;
}

void DataViewCommon::ForEachVariableAddress(const Function<void(const DataAddress&)>& function) const
{
	RMLUI_ASSERT(expression);
	for (const DataAddress& address : expression->GetAddresses())
		function(address);
}

const String& DataViewCommon::GetModifier() const
{
	return modifier;
}

DataExpression& DataViewCommon::GetExpression()
{
	RMLUI_ASSERT(expression);
	return *expression;
}

void DataViewCommon::Release()
{
	delete this;
}

DataViewAttribute::DataViewAttribute(Element* element) : DataViewCommon(element) {}

DataViewAttribute::DataViewAttribute(Element* element, String override_attribute, int sort_offset) :
	DataViewCommon(element, std::move(override_attribute), sort_offset)
{}

bool DataViewAttribute::Update(DataModel& model)
{
	const String& attribute_name = GetModifier();
	bool result = false;
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (element && GetExpression().Run(expr_interface, variant))
	{
		const String value = variant.Get<String>();
		const Variant* attribute = element->GetAttribute(attribute_name);

		if (!attribute || attribute->Get<String>() != value)
		{
			element->SetAttribute(attribute_name, value);
			result = true;
		}
	}
	return result;
}

DataViewAttributeIf::DataViewAttributeIf(Element* element) : DataViewCommon(element) {}

bool DataViewAttributeIf::Update(DataModel& model)
{
	const String& attribute_name = GetModifier();
	bool result = false;
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (element && GetExpression().Run(expr_interface, variant))
	{
		const bool value = variant.Get<bool>();
		const bool is_set = static_cast<bool>(element->GetAttribute(attribute_name));
		if (is_set != value)
		{
			if (value)
				element->SetAttribute(attribute_name, String());
			else
				element->RemoveAttribute(attribute_name);
			result = true;
		}
	}
	return result;
}

DataViewValue::DataViewValue(Element* element) : DataViewAttribute(element, "value", SortOffset_DataValue) {}

DataViewChecked::DataViewChecked(Element* element) : DataViewCommon(element, String(), SortOffset_DataChecked) {}

bool DataViewChecked::Update(DataModel& model)
{
	bool result = false;
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (element && GetExpression().Run(expr_interface, variant))
	{
		bool new_checked_state = false;

		if (variant.GetType() == Variant::BOOL)
		{
			new_checked_state = variant.Get<bool>();
		}
		else
		{
			const String value = variant.Get<String>();
			new_checked_state = (!value.empty() && value == element->GetAttribute<String>("value", ""));
		}

		const bool current_checked_state = element->HasAttribute("checked");

		if (new_checked_state != current_checked_state)
		{
			result = true;
			if (new_checked_state)
				element->SetAttribute("checked", String());
			else
				element->RemoveAttribute("checked");
		}
	}

	return result;
}

DataViewStyle::DataViewStyle(Element* element) : DataViewCommon(element) {}

bool DataViewStyle::Update(DataModel& model)
{
	const String& property_name = GetModifier();
	bool result = false;
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (element && GetExpression().Run(expr_interface, variant))
	{
		const String value = variant.Get<String>();
		const Property* p = element->GetLocalProperty(property_name);
		if (!p || p->Get<String>() != value)
		{
			element->SetProperty(property_name, value);
			result = true;
		}
	}
	return result;
}

DataViewClass::DataViewClass(Element* element) : DataViewCommon(element) {}

bool DataViewClass::Update(DataModel& model)
{
	const String& class_name = GetModifier();
	bool result = false;
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (element && GetExpression().Run(expr_interface, variant))
	{
		const bool activate = variant.Get<bool>();
		const bool is_set = element->IsClassSet(class_name);
		if (activate != is_set)
		{
			element->SetClass(class_name, activate);
			result = true;
		}
	}
	return result;
}

DataViewRml::DataViewRml(Element* element) : DataViewCommon(element) {}

bool DataViewRml::Update(DataModel& model)
{
	bool result = false;
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (element && GetExpression().Run(expr_interface, variant))
	{
		String new_rml = variant.Get<String>();
		if (new_rml != previous_rml)
		{
			element->SetInnerRML(new_rml);
			previous_rml = std::move(new_rml);
			result = true;
		}
	}
	return result;
}

DataViewIf::DataViewIf(Element* element) : DataViewCommon(element, String(), SortOffset_Structural) {}

bool DataViewIf::Initialize(DataModel& model, Element* element, const String& expression, const String& modifier)
{
	if (!DataViewCommon::Initialize(model, element, expression, modifier))
		return false;

	element->SetProperty(PropertyId::Display, Property(Style::Display::None));

	for (const ElementAttributes::value_type& attribute : element->GetAttributes())
	{
		if (attribute.first == "data-if" || attribute.first == "rmlui-inner-rml")
			continue;
		attributes.emplace(attribute.first, attribute.second);
	}

	return true;
}

bool DataViewIf::Update(DataModel& model)
{
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (!element || !GetExpression().Run(expr_interface, variant))
		return false;

	const bool value = variant.Get<bool>();
	Element* current_branch = branch.get();
	if (value == (current_branch != nullptr))
		return false;

	if (!value)
	{
		if (Element* branch_parent = current_branch->GetParentNode())
			branch_parent->RemoveChild(current_branch).reset();
		branch.reset();
		return true;
	}

	Element* parent = element->GetParentNode();
	if (!parent)
		return false;

	ElementPtr new_branch = Factory::InstanceElement(nullptr, element->GetTagName(), element->GetTagName(), attributes);
	if (!new_branch)
	{
		Log::Message(Log::LT_WARNING, "Could not instance data-if branch of element %s", element->GetAddress().c_str());
		return false;
	}

	Element* inserted_branch = parent->InsertBefore(std::move(new_branch), element);
	branch = inserted_branch->GetObserverPtr();

	if (const SharedPtr<const XMLEventStream>& compiled_inner_rml = CompiledInnerRmlAccess::Get(*element))
	{
		XMLEventReplay::InstanceInnerRml(inserted_branch, compiled_inner_rml);
	}
	else
	{
		const Variant* rml_contents = element->GetAttribute("rmlui-inner-rml");
		if (rml_contents && rml_contents->GetType() == Variant::STRING)
			inserted_branch->SetInnerRML(rml_contents->GetReference<String>());
	}

	return true;
}

DataViewVisible::DataViewVisible(Element* element) : DataViewCommon(element) {}

bool DataViewVisible::Update(DataModel& model)
{
	bool result = false;
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (element && GetExpression().Run(expr_interface, variant))
	{
		const bool value = variant.Get<bool>();
		const bool is_visible = (element->GetLocalStyleProperties().count(PropertyId::Visibility) == 0);
		if (is_visible != value)
		{
			if (value)
				element->RemoveProperty(PropertyId::Visibility);
			else
				element->SetProperty(PropertyId::Visibility, Property(Style::Visibility::Hidden));
			result = true;
		}
	}
	return result;
}

namespace DataViewWorldText {
	static bool ParseWorldPoint(const String& text, WorldPoint& out_point)
	{
		double values[3] = {};
		const char* cursor = text.c_str();
		for (int i = 0; i < 3; i++)
		{
			if (i > 0 && !StringUtilities::IsWhitespace(*cursor))
				return false;

			char* end = nullptr;
			values[i] = std::strtod(cursor, &end);
			if (end == cursor || !std::isfinite(values[i]))
				return false;
			cursor = end;
		}

		while (StringUtilities::IsWhitespace(*cursor))
			cursor++;
		if (*cursor != '\0')
			return false;

		out_point = WorldPoint(values[0], values[1], values[2]);
		return true;
	}
} // namespace DataViewWorldText

DataViewWorld::DataViewWorld(Element* element) : DataViewCommon(element) {}

bool DataViewWorld::Initialize(DataModel& model, Element* element, const String& expression, const String& modifier)
{
	if (!DataViewCommon::Initialize(model, element, expression, modifier))
		return false;

	Context* context = element->GetContext();
	if (!context)
		return false;

	anchor_set = &WorldAnchorSet::Get(*context);
	anchor_set->Add(this);
	Place();
	return true;
}

bool DataViewWorld::Update(DataModel& model)
{
	Variant variant;
	Element* element = GetElement();
	DataExpressionInterface expr_interface(&model, element);

	if (!element || !GetExpression().Run(expr_interface, variant))
		return false;

	const String text = variant.Get<String>();
	has_world_point = DataViewWorldText::ParseWorldPoint(text, world_point);
	if (!has_world_point && !parse_error_logged)
	{
		parse_error_logged = true;
		Log::Message(Log::LT_ERROR, "data-world on element %s: '%s' is not a world point \"x y z\".", element->GetAddress().c_str(), text.c_str());
	}

	return Place();
}

bool DataViewWorld::Place()
{
	Element* element = GetElement();
	if (!element || !anchor_set)
		return false;

	Vector2f point;
	const bool visible = has_world_point && anchor_set->Project(world_point, point);
	bool changed = false;

	if (visible && (!has_applied_translate || point != applied_translate))
	{
		element->SetProperty(PropertyId::Transform, Transform::MakeProperty({Transforms::Translate2D(point.x, point.y, Unit::DEVICE_PX)}));
		applied_translate = point;
		has_applied_translate = true;
		changed = true;
	}

	if (visible != applied_visible)
	{
		if (visible)
			element->RemoveProperty(PropertyId::Visibility);
		else
			element->SetProperty(PropertyId::Visibility, Property(Style::Visibility::Hidden));
		applied_visible = visible;
		changed = true;
	}

	return changed;
}

void DataViewWorld::Release()
{
	if (anchor_set)
		anchor_set->Remove(this);
	DataViewCommon::Release();
}

DataViewText::DataViewText(Element* element) : DataView(element, 0) {}

bool DataViewText::Initialize(DataModel& model, Element* element, const String& /*expression*/, const String& /*modifier*/)
{
	ElementText* element_text = rmlui_dynamic_cast<ElementText*>(element);
	if (!element_text)
		return false;

	const String& in_text = element_text->GetText();
	source = in_text;

	Vector<DataTextExpression> expressions;
	if (const char* error_str = SplitDataText(in_text, text, expressions))
	{
		Log::Message(Log::LT_WARNING, "Failed to parse data view text '%s'. %s", in_text.c_str(), error_str);
		return false;
	}

	DataExpressionInterface expression_interface(&model, element);
	const bool is_assignment = DataExpressionTable::IsAssignment(DataExpressionSite::ViewText);

	for (DataTextExpression& text_expression : expressions)
	{
		DataEntry entry;
		entry.index = text_expression.index;
		entry.data_expression = MakeUnique<DataExpression>();
		entry.value = "#rmlui#"; // A random value that the user string will not be initialized with.

		if (entry.data_expression->Parse(text_expression.expression, expression_interface, is_assignment))
			data_entries.push_back(std::move(entry));
	}

	if (data_entries.empty())
		return false;

	return true;
}

bool DataViewText::Update(DataModel& model)
{
	bool entries_modified = false;
	{
		Element* element = GetElement();
		DataExpressionInterface expression_interface(&model, element);

		for (DataEntry& entry : data_entries)
		{
			RMLUI_ASSERT(entry.data_expression);
			Variant variant;
			bool result = entry.data_expression->Run(expression_interface, variant);
			const String value = variant.Get<String>();
			if (result && entry.value != value)
			{
				entry.value = value;
				entries_modified = true;
			}
		}
	}

	if (entries_modified)
	{
		if (Element* element = GetElement())
		{
			String new_text = BuildText();
			String text;
			if (SystemInterface* system_interface = GetSystemInterface())
				system_interface->TranslateString(text, new_text);

			rmlui_static_cast<ElementText*>(element)->SetText(text);
		}
		else
		{
			Log::Message(Log::LT_WARNING, "Could not update data view text, element no longer valid. Was it destroyed?");
		}
	}

	return entries_modified;
}

void DataViewText::ForEachVariableAddress(const Function<void(const DataAddress&)>& function) const
{
	for (const DataEntry& entry : data_entries)
	{
		RMLUI_ASSERT(entry.data_expression);
		for (const DataAddress& address : entry.data_expression->GetAddresses())
			function(address);
	}
}

void DataViewText::RestoreElementSource()
{
	if (ElementText* element_text = rmlui_dynamic_cast<ElementText*>(GetElement()))
		element_text->SetText(source);
}

void DataViewText::Release()
{
	delete this;
}

String DataViewText::BuildText() const
{
	size_t reserve_size = text.size();

	for (const DataEntry& entry : data_entries)
		reserve_size += entry.value.size();

	String result;
	result.reserve(reserve_size);

	size_t previous_index = 0;
	for (const DataEntry& entry : data_entries)
	{
		result += text.substr(previous_index, entry.index - previous_index);
		result += entry.value;
		previous_index = entry.index;
	}

	if (previous_index < text.size())
		result += text.substr(previous_index);

	return result;
}

DataViewFor::DataViewFor(Element* element) : DataView(element, SortOffset_Structural) {}

bool DataViewFor::Initialize(DataModel& model, Element* element, const String& in_expression, const String& /*modifier*/)
{
	StringList iterator_container_pair;
	StringUtilities::ExpandString(iterator_container_pair, in_expression, ':');

	if (iterator_container_pair.empty() || iterator_container_pair.size() > 2 || iterator_container_pair.front().empty() ||
		iterator_container_pair.back().empty())
	{
		Log::Message(Log::LT_WARNING, "Invalid syntax in data-for '%s'", in_expression.c_str());
		return false;
	}

	if (iterator_container_pair.size() == 2)
	{
		StringList iterator_index_pair;
		StringUtilities::ExpandString(iterator_index_pair, iterator_container_pair.front(), ',');

		if (iterator_index_pair.empty())
		{
			Log::Message(Log::LT_WARNING, "Invalid syntax in data-for '%s'", in_expression.c_str());
			return false;
		}
		else if (iterator_index_pair.size() == 1)
		{
			iterator_name = iterator_index_pair.front();
		}
		else if (iterator_index_pair.size() == 2)
		{
			iterator_name = iterator_index_pair.front();
			iterator_index_name = iterator_index_pair.back();
		}
	}

	if (iterator_name.empty())
		iterator_name = "it";

	if (iterator_index_name.empty())
		iterator_index_name = "it_index";

	const String& container_name = iterator_container_pair.back();

	container_address = model.ResolveAddress(container_name, element);
	if (container_address.empty())
		return false;

	if (const Variant* key_attribute = element->GetAttribute("data-key"))
	{
		if (!InitializeKey(element, key_attribute->Get<String>()))
			return false;
	}

	if (element->GetAttribute("data-virtual"))
	{
		virtual_state = VirtualState::Unchecked;
		virtual_model = &model;
	}

	element->SetProperty(PropertyId::Display, Property(Style::Display::None));

	// Copy over the attributes, but remove the 'data-for' which would otherwise recreate the data-for loop on all
	// constructed children recursively. There is also no need for the 'rmlui-inner-rml' attribute if that is present.
	const ElementAttributes& element_attributes = element->GetAttributes();
	constexpr size_t num_data_for_attributes = 1;
	if (element_attributes.size() < num_data_for_attributes)
	{
		Log::Message(Log::LT_WARNING, "Expected attributes for data-for view missing from element: %s", element->GetAddress().c_str());
		return false;
	}
	attributes.reserve(element_attributes.size() - num_data_for_attributes);
	for (const auto& attribute : element->GetAttributes())
	{
		if (attribute.first == "data-for" || attribute.first == "data-if" || attribute.first == "data-key" || attribute.first == "data-virtual" ||
			attribute.first == "rmlui-inner-rml")
			continue;
		attributes.emplace(attribute.first, attribute.second);
	}

	return true;
}

bool DataViewFor::InitializeKey(Element* element, const String& in_key_text)
{
	StringList segments;
	size_t segment_begin = 0;
	while (true)
	{
		const size_t segment_end = in_key_text.find('.', segment_begin);
		segments.push_back(in_key_text.substr(segment_begin, segment_end == String::npos ? String::npos : segment_end - segment_begin));
		if (segment_end == String::npos)
			break;
		segment_begin = segment_end + 1;
	}

	bool valid = (segments.size() >= 2 && segments.front() == iterator_name);
	for (size_t i = 1; valid && i < segments.size(); i++)
	{
		const String& segment = segments[i];
		valid = !segment.empty() && !(segment[0] >= '0' && segment[0] <= '9');
		for (const char c : segment)
			valid &= (c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'));
	}

	if (!valid)
	{
		Log::Message(Log::LT_ERROR, "Invalid data-key '%s' on %s; expected '%s.<member path>'.", in_key_text.c_str(), element->GetAddress().c_str(),
			iterator_name.c_str());
		return false;
	}

	key_text = in_key_text;
	key_path.reserve(segments.size() - 1);
	for (size_t i = 1; i < segments.size(); i++)
		key_path.push_back(DataAddressEntry(segments[i]));

	return true;
}

bool DataViewFor::KeyedRowContentValid(Element* anchor) const
{
	bool has_structural_view = false;
	if (const SharedPtr<const XMLEventStream>& compiled_inner_rml = CompiledInnerRmlAccess::Get(*anchor))
	{
		has_structural_view = HasStructuralDataView(*compiled_inner_rml);
	}
	else if (const Variant* rml_contents = anchor->GetAttribute("rmlui-inner-rml"))
	{
		if (rml_contents->GetType() == Variant::STRING)
			has_structural_view = HasStructuralDataView(rml_contents->GetReference<String>());
	}

	if (has_structural_view)
		Log::Message(Log::LT_ERROR, "Keyed data-for on %s: a keyed row cannot contain data-for or data-if; no rows are created.",
			anchor->GetAddress().c_str());

	return !has_structural_view;
}

void DataViewFor::InsertRowAliases(DataModel& model, Element* row, int index) const
{
	DataAddress iterator_address;
	iterator_address.reserve(container_address.size() + 1);
	iterator_address = container_address;
	iterator_address.push_back(DataAddressEntry(index));

	DataAddress iterator_index_address = {{"literal"}, {"int"}, {index}};

	model.InsertAlias(row, iterator_name, std::move(iterator_address));
	model.InsertAlias(row, iterator_index_name, std::move(iterator_index_address));
}

Element* DataViewFor::InstanceRow(DataModel& model, Element* anchor, Element* before, int index) const
{
	ElementPtr new_element_ptr = Factory::InstanceElement(nullptr, anchor->GetTagName(), anchor->GetTagName(), attributes);

	InsertRowAliases(model, new_element_ptr.get(), index);

	Element* new_element = anchor->GetParentNode()->InsertBefore(std::move(new_element_ptr), before);

	if (const SharedPtr<const XMLEventStream>& compiled_inner_rml = CompiledInnerRmlAccess::Get(*anchor))
	{
		XMLEventReplay::InstanceInnerRml(new_element, compiled_inner_rml);
	}
	else
	{
		const String* rml_contents = RMLContents();
		new_element->SetInnerRML(rml_contents ? *rml_contents : "");
	}

	return new_element;
}

bool DataViewFor::UpdateKeyed(DataModel& model, const DataVariable& variable, Element* anchor, Element* end_marker, int begin, int end)
{
	if (keyed_row_content == KeyedRowContent::Unchecked)
		keyed_row_content = KeyedRowContentValid(anchor) ? KeyedRowContent::Valid : KeyedRowContent::Invalid;
	if (keyed_row_content == KeyedRowContent::Invalid)
		return false;

	const int count = end - begin;
	const int num_old_rows = (int)keyed_rows.size();

	Vector<String> keys(count);
	Vector<bool> has_row(count, false);
	UnorderedMap<String, int> entry_by_key;
	entry_by_key.reserve(count);
	int first_unreadable = -1;
	int first_duplicate = -1;

	for (int j = 0; j < count; j++)
	{
		const int i = begin + j;
		DataVariable key_variable = variable.Child(DataAddressEntry(i));
		for (const DataAddressEntry& entry : key_path)
		{
			if (!key_variable)
				break;
			key_variable = key_variable.Child(entry);
		}

		Variant key_value;
		if (!key_variable || !key_variable.Get(key_value))
		{
			if (first_unreadable < 0)
				first_unreadable = i;
			continue;
		}

		keys[j] = key_value.Get<String>();
		if (!entry_by_key.emplace(keys[j], j).second)
		{
			if (first_duplicate < 0)
				first_duplicate = i;
			continue;
		}
		has_row[j] = true;
	}

	if (first_unreadable >= 0)
		Log::Message(Log::LT_ERROR, "Keyed data-for on %s: data-key '%s' cannot be read from entry %d; entries without a key get no row.",
			anchor->GetAddress().c_str(), key_text.c_str(), first_unreadable);
	if (first_duplicate >= 0)
		Log::Message(Log::LT_ERROR, "Keyed data-for on %s: entry %d repeats data-key value '%s'; later entries with a repeated key get no row.",
			anchor->GetAddress().c_str(), first_duplicate, keys[first_duplicate - begin].c_str());

	Vector<int> kept_row_of_entry(count, -1);
	Vector<bool> row_kept(num_old_rows, false);
	for (int row = 0; row < num_old_rows; row++)
	{
		const UnorderedMap<String, int>::const_iterator it = entry_by_key.find(keyed_rows[row].key);
		if (it != entry_by_key.end())
		{
			kept_row_of_entry[it->second] = row;
			row_kept[row] = true;
		}
	}

	int previous_kept_row = -1;
	bool out_of_order = false;
	for (int j = 0; j < count; j++)
	{
		const int row = kept_row_of_entry[j];
		if (row < 0)
			continue;
		out_of_order |= (row < previous_kept_row);
		if (out_of_order)
		{
			row_kept[row] = false;
			kept_row_of_entry[j] = -1;
			continue;
		}
		previous_kept_row = row;
	}

	bool result = false;

	for (int row = 0; row < num_old_rows; row++)
	{
		if (row_kept[row])
			continue;
		Element* row_element = keyed_rows[row].element;
		model.EraseAliases(row_element);
		row_element->GetParentNode()->RemoveChild(row_element).reset();
		result = true;
	}

	Vector<Element*> insert_before(count, end_marker);
	Element* next_kept = end_marker;
	for (int j = count - 1; j >= 0; j--)
	{
		insert_before[j] = next_kept;
		if (kept_row_of_entry[j] >= 0)
			next_kept = keyed_rows[kept_row_of_entry[j]].element;
	}

	Vector<KeyedRow> new_rows;
	new_rows.reserve(entry_by_key.size());
	for (int j = 0; j < count; j++)
	{
		if (!has_row[j])
			continue;

		const int i = begin + j;
		const int row = kept_row_of_entry[j];
		if (row >= 0)
		{
			KeyedRow kept = std::move(keyed_rows[row]);
			if (kept.index != i)
			{
				Element* row_element = kept.element;
				model.RebindSubtree(row_element, [this, row_element, i](DataModel& rebind_model) { InsertRowAliases(rebind_model, row_element, i); });
				kept.index = i;
				result = true;
			}
			new_rows.push_back(std::move(kept));
		}
		else
		{
			KeyedRow fresh;
			fresh.element = InstanceRow(model, anchor, insert_before[j], i);
			fresh.key = std::move(keys[j]);
			fresh.index = i;
			new_rows.push_back(std::move(fresh));
			result = true;
		}
	}

	keyed_rows = std::move(new_rows);

	return result;
}

bool DataViewFor::UpdateRows(DataModel& model, Element* anchor, Element* end_marker, int begin, int end)
{
	const int old_begin = window_begin;
	const int num_old = (int)elements.size();
	const int old_end = old_begin + num_old;
	if (begin == old_begin && end == old_end)
		return false;

	int keep_begin = Math::Max(begin, old_begin);
	int keep_end = Math::Min(end, old_end);
	if (keep_begin >= keep_end)
	{
		keep_begin = begin;
		keep_end = begin;
	}

	for (int k = 0; k < num_old; k++)
	{
		const int index = old_begin + k;
		if (index >= keep_begin && index < keep_end)
			continue;
		model.EraseAliases(elements[k]);
		elements[k]->GetParentNode()->RemoveChild(elements[k]).reset();
	}

	ElementList new_elements;
	new_elements.reserve(end - begin);
	Element* first_kept = (keep_begin < keep_end ? elements[keep_begin - old_begin] : end_marker);
	for (int i = begin; i < keep_begin; i++)
		new_elements.push_back(InstanceRow(model, anchor, first_kept, i));
	for (int i = keep_begin; i < keep_end; i++)
		new_elements.push_back(elements[i - old_begin]);
	for (int i = keep_end; i < end; i++)
		new_elements.push_back(InstanceRow(model, anchor, end_marker, i));

	elements = std::move(new_elements);
	window_begin = begin;

	return true;
}

Element* DataViewFor::FirstRow() const
{
	if (!key_path.empty())
		return keyed_rows.empty() ? nullptr : keyed_rows.front().element;
	return elements.empty() ? nullptr : elements.front();
}

bool DataViewFor::SetupVirtual(DataModel& model, Element* anchor)
{
	Element* container = scroll_container.get();
	if (!container)
	{
		container = anchor->GetParentNode();
		if (!container)
			return false;
		scroll_container = container->GetObserverPtr();
		container->AddEventListener(EventId::Scroll, this);
		model.AddLayoutObserver(this);
	}

	for (ObserverPtr<Element>* spacer : {&leading_spacer, &trailing_spacer})
	{
		if (*spacer)
			continue;

		ElementPtr spacer_ptr = Factory::InstanceElement(nullptr, "div", "div", XMLAttributes());
		spacer_ptr->SetClass("rml-virtual-spacer", true);
		spacer_ptr->SetProperty(PropertyId::Display, Property(Style::Display::None));
		spacer_ptr->SetProperty(PropertyId::Width, Property(100.f, Unit::PERCENT));
		spacer_ptr->SetProperty(PropertyId::FlexShrink, Property(0.f, Unit::NUMBER));
		spacer_ptr->SetProperty("grid-column", "1 / -1");

		Element* before = anchor;
		if (spacer == &leading_spacer)
		{
			if (Element* first_row = FirstRow())
				before = first_row;
			else if (Element* trailing = trailing_spacer.get())
				before = trailing;
		}

		*spacer = container->InsertBefore(std::move(spacer_ptr), before)->GetObserverPtr();
		if (spacer == &leading_spacer)
			applied_leading_height = -1.f;
		else
			applied_trailing_height = -1.f;
	}

	return true;
}

void DataViewFor::ComputeWindow(int size, int& out_begin, int& out_end) const
{
	out_begin = 0;
	out_end = 0;
	if (virtual_state == VirtualState::Invalid || size <= 0)
		return;

	Element* container = scroll_container.get();
	if (!measured || !container)
	{
		out_end = 1;
		return;
	}

	const int lines = (size + columns - 1) / columns;
	const float client_height = Math::Max(container->GetClientHeight(), 0.f);
	const float view_top = container->GetScrollTop() - container->GetBox().GetEdge(BoxArea::Padding, BoxEdge::Top);
	const float view_bottom = view_top + client_height;

	const int first_visible = Math::Clamp((int)std::floor(view_top / line_size), 0, lines - 1);
	const int last_visible = Math::Clamp((int)std::floor(view_bottom / line_size), first_visible, lines - 1);
	const int overscan = Math::Max(1, (int)std::ceil(client_height / line_size));

	const int first_line = Math::Max(0, first_visible - overscan);
	const int last_line = Math::Min(lines - 1, last_visible + overscan);

	out_begin = first_line * columns;
	out_end = Math::Min(size, (last_line + 1) * columns);
}

bool DataViewFor::UpdateSpacer(Element* spacer, int lines, float& applied_height) const
{
	if (!spacer)
		return false;

	const float height = (lines > 0 ? Math::Max(float(lines) * line_size - row_gap, 0.f) : -1.f);
	if (height == applied_height)
		return false;

	if (height < 0.f)
	{
		spacer->SetProperty(PropertyId::Display, Property(Style::Display::None));
	}
	else
	{
		spacer->SetProperty(PropertyId::Display, Property(Style::Display::Block));
		spacer->SetProperty(PropertyId::Height, Property(height, Unit::DEVICE_PX));
	}
	applied_height = height;
	return true;
}

bool DataViewFor::UpdateVirtual(DataModel& model, const DataVariable& variable, Element* anchor)
{
	window_requested = false;

	if (!SetupVirtual(model, anchor))
		return false;

	const int size = variable.Size();
	int begin = 0;
	int end = 0;
	ComputeWindow(size, begin, end);

	Element* end_marker = trailing_spacer.get();
	bool result = (key_path.empty() ? UpdateRows(model, anchor, end_marker, begin, end) : UpdateKeyed(model, variable, anchor, end_marker, begin, end));

	const bool spaced = (measured && virtual_state == VirtualState::Valid && end > begin);
	const int lines = (spaced ? (size + columns - 1) / columns : 0);
	const int leading_lines = (spaced ? begin / columns : 0);
	const int trailing_lines = (spaced ? lines - (end + columns - 1) / columns : 0);
	result |= UpdateSpacer(leading_spacer.get(), leading_lines, applied_leading_height);
	result |= UpdateSpacer(trailing_spacer.get(), trailing_lines, applied_trailing_height);

	applied_begin = begin;
	applied_end = end;
	applied_columns = columns;
	applied_line_size = line_size;
	applied_row_gap = row_gap;

	return result;
}

bool DataViewFor::RequestWindowUpdate(DataModel& model)
{
	if (!window_requested)
	{
		window_requested = true;
		model.RequestViewUpdate(this);
	}
	return true;
}

bool DataViewFor::OnLayout(DataModel& model)
{
	Element* anchor = GetElement();
	Element* container = scroll_container.get();
	if (!anchor || !container || virtual_state == VirtualState::Off || virtual_state == VirtualState::Invalid)
		return false;

	if (virtual_state == VirtualState::Unchecked)
	{
		const Style::Overflow overflow = container->GetComputedValues().overflow_y();
		if (overflow != Style::Overflow::Auto && overflow != Style::Overflow::Scroll)
		{
			Log::Message(Log::LT_ERROR, "Virtual data-for on %s: its parent must scroll vertically (overflow-y: auto or scroll); no rows are created.",
				anchor->GetAddress().c_str());
			virtual_state = VirtualState::Invalid;
			return RequestWindowUpdate(model);
		}
		virtual_state = VirtualState::Valid;
	}

	Element* first_row = FirstRow();
	if (!first_row)
		return false;

	const Box& container_box = container->GetBox();
	const ComputedValues& container_values = container->GetComputedValues();
	const Style::Display display = container_values.display();
	const bool has_gaps = (display == Style::Display::Flex || display == Style::Display::InlineFlex || display == Style::Display::Grid ||
		display == Style::Display::InlineGrid);
	const float content_width = container->GetClientWidth() - container_box.GetEdge(BoxArea::Padding, BoxEdge::Left) -
		container_box.GetEdge(BoxArea::Padding, BoxEdge::Right);
	const float column_gap = (has_gaps ? ResolveValue(container_values.column_gap(), content_width) : 0.f);
	const float new_row_gap = (has_gaps ? ResolveValue(container_values.row_gap(), container_box.GetSize(BoxArea::Content).y) : 0.f);
	const Vector2f item_size = first_row->GetBox().GetSize(BoxArea::Margin);

	const float new_line_size = item_size.y + new_row_gap;
	if (new_line_size <= 0.f)
		return false;

	const float column_pitch = item_size.x + column_gap;
	const int new_columns = (column_pitch > 0.f ? Math::Max(1, (int)std::floor((content_width + column_gap) / column_pitch + 0.01f)) : 1);

	measured = true;
	columns = new_columns;
	line_size = new_line_size;
	row_gap = new_row_gap;

	const DataVariable variable = model.GetVariable(container_address);
	if (!variable)
		return false;

	int begin = 0;
	int end = 0;
	ComputeWindow(variable.Size(), begin, end);

	if (begin == applied_begin && end == applied_end && columns == applied_columns && line_size == applied_line_size && row_gap == applied_row_gap)
		return false;

	return RequestWindowUpdate(model);
}

void DataViewFor::ProcessEvent(Event& event)
{
	Element* container = scroll_container.get();
	if (!container || event.GetTargetElement() != container || !virtual_model || virtual_state != VirtualState::Valid)
		return;

	// Scroll is dispatched synchronously inside SetScrollTop (Element.cpp:1180); rows change only in the next data model update.
	RequestWindowUpdate(*virtual_model);
	if (Context* context = container->GetContext())
		context->RequestNextUpdate(0);
}

bool DataViewFor::Update(DataModel& model)
{
	DataVariable variable = model.GetVariable(container_address);
	if (!variable)
		return false;

	Element* anchor = GetElement();

	if (virtual_state != VirtualState::Off)
		return UpdateVirtual(model, variable, anchor);

	if (!key_path.empty())
		return UpdateKeyed(model, variable, anchor, anchor, 0, variable.Size());

	return UpdateRows(model, anchor, anchor, 0, variable.Size());
}

void DataViewFor::ForEachVariableAddress(const Function<void(const DataAddress&)>& function) const
{
	RMLUI_ASSERT(!container_address.empty());
	function(container_address);
}

void DataViewFor::Release()
{
	if (Element* container = scroll_container.get())
		container->RemoveEventListener(EventId::Scroll, this);
	delete this;
}

const String* DataViewFor::RMLContents() const
{
	if (Element* element = GetElement())
	{
		if (Variant* attribute = element->GetAttribute("rmlui-inner-rml"))
		{
			if (attribute->GetType() == Variant::STRING)
				return &attribute->GetReference<String>();
		}
		Log::Message(Log::LT_WARNING, "Missing or invalid RML contents in data-for view on element %s", element->GetAddress().c_str());
	}
	else
	{
		Log::Message(Log::LT_WARNING, "Invalid element in data-for view");
	}
	return nullptr;
}

DataViewAlias::DataViewAlias(Element* element) : DataView(element, 0) {}

void DataViewAlias::ForEachVariableAddress(const Function<void(const DataAddress&)>& /*function*/) const {}

bool DataViewAlias::Update(DataModel&)
{
	return false;
}

bool DataViewAlias::Initialize(DataModel& model, Element* element, const String& expression, const String& modifier)
{
	auto address = model.ResolveAddress(expression, element);
	if (address.empty())
		return false;

	model.InsertAlias(element, modifier, address);
	return true;
}

void DataViewAlias::Release()
{
	delete this;
}

} // namespace Rml
