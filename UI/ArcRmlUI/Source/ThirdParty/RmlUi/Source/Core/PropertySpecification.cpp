#include "../../Include/RmlUi/Core/PropertySpecification.h"
#include "../../Include/RmlUi/Core/Debug.h"
#include "../../Include/RmlUi/Core/Log.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/PropertyDictionary.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "IdNameMap.h"
#include "PropertyParserGrid.h"
#include "PropertyShorthandDefinition.h"
#include <algorithm>
#include <limits.h>
#include <stdint.h>

namespace Rml {

namespace PropertySpecificationPosition {
	enum class PositionAxis { Horizontal, Vertical, Either };

	struct PositionToken {
		bool keyword = false;
		bool far_edge = false;
		PositionAxis axis = PositionAxis::Either;
		String value;
	};

	static PositionToken ClassifyPositionToken(const String& token)
	{
		const String lower = StringUtilities::ToLower(token);
		PositionToken result;
		result.keyword = true;
		if (lower == "left" || lower == "right")
		{
			result.axis = PositionAxis::Horizontal;
			result.far_edge = (lower == "right");
		}
		else if (lower == "top" || lower == "bottom")
		{
			result.axis = PositionAxis::Vertical;
			result.far_edge = (lower == "bottom");
		}
		else if (lower != "center")
		{
			result.keyword = false;
			result.value = token;
			return result;
		}
		result.value = (lower == "center" ? "50%" : (result.far_edge ? "100%" : "0%"));
		return result;
	}
} // namespace PropertySpecificationPosition

static void SetShorthandPropertiesToPendingSubstitution(PropertyDictionary& dictionary, const ShorthandDefinition* shorthand_definition)
{
	for (const ShorthandItem& shorthand_item : shorthand_definition->items)
	{
		if (shorthand_item.type == ShorthandItemType::Property)
		{
			Property property;
			property.unit = Unit::SHORTHAND_PLACEHOLDER;
			dictionary.SetProperty(shorthand_item.property_id, property);
		}
		else if (shorthand_item.type == ShorthandItemType::Shorthand)
		{
			SetShorthandPropertiesToPendingSubstitution(dictionary, shorthand_item.shorthand_definition);
		}
	}
}

static void SetShorthandPropertiesToCssWideKeyword(PropertyDictionary& dictionary, const ShorthandDefinition* shorthand_definition,
	const String& keyword)
{
	for (const ShorthandItem& shorthand_item : shorthand_definition->items)
	{
		if (shorthand_item.type == ShorthandItemType::Property)
		{
			Property property;
			if (shorthand_item.property_definition->ParseValue(property, keyword))
				dictionary.SetProperty(shorthand_item.property_id, property);
		}
		else if (shorthand_item.type == ShorthandItemType::Shorthand)
		{
			SetShorthandPropertiesToCssWideKeyword(dictionary, shorthand_item.shorthand_definition, keyword);
		}
	}
}

PropertySpecification::PropertySpecification(size_t reserve_num_properties, size_t reserve_num_shorthands) :
	// Increment reserve numbers by one because the 'invalid' property occupies the first element
	properties(reserve_num_properties + 1), shorthands(reserve_num_shorthands + 1),
	property_map(MakeUnique<PropertyIdNameMap>(reserve_num_properties + 1)), shorthand_map(MakeUnique<ShorthandIdNameMap>(reserve_num_shorthands + 1))
{}

PropertySpecification::~PropertySpecification() {}

PropertyDefinition& PropertySpecification::RegisterProperty(const String& property_name, const String& default_value, bool inherited,
	bool forces_layout, PropertyId id)
{
	if (id == PropertyId::Invalid)
		id = property_map->GetOrCreateId(property_name);
	else
		property_map->AddPair(id, property_name);

	size_t index = (size_t)id;

	if (index >= size_t(PropertyId::MaxNumIds))
	{
		Log::Message(Log::LT_ERROR,
			"Fatal error while registering property '%s': Maximum number of allowed properties exceeded. Continuing execution may lead to crash.",
			property_name.c_str());
		RMLUI_ERROR;
		return *properties[0];
	}

	if (index < properties.size())
	{
		// We don't want to owerwrite an existing entry.
		if (properties[index])
		{
			Log::Message(Log::LT_ERROR, "While registering property '%s': The property is already registered.", property_name.c_str());
			return *properties[index];
		}
	}
	else
	{
		// Resize vector to hold the new index
		properties.resize((index * 3) / 2 + 1);
	}

	// Create and insert the new property
	properties[index] = MakeUnique<PropertyDefinition>(id, default_value, inherited, forces_layout);
	property_ids.Insert(id);
	if (inherited)
		property_ids_inherited.Insert(id);
	if (forces_layout)
		property_ids_forcing_layout.Insert(id);

	return *properties[index];
}

const PropertyDefinition* PropertySpecification::GetProperty(PropertyId id) const
{
	if (id == PropertyId::Invalid || (size_t)id >= properties.size())
		return nullptr;

	return properties[(size_t)id].get();
}

const PropertyDefinition* PropertySpecification::GetProperty(const String& property_name) const
{
	return GetProperty(property_map->GetId(property_name));
}

const String& PropertySpecification::GetPropertyName(PropertyId id) const
{
	return property_map->GetName(id);
}

const String& PropertySpecification::GetShorthandName(ShorthandId id) const
{
	return shorthand_map->GetName(id);
}

String PropertySpecification::GetRegistrySignature() const
{
	StringList lines;

	for (const UniquePtr<PropertyDefinition>& definition : properties)
	{
		if (!definition)
			continue;

		String line = "p " + property_map->GetName(definition->GetId()) + (definition->GetPreserveQuotes() ? " q" : "");
		for (const PropertyDefinition::ParserState& parser_state : definition->parsers)
		{
			Vector<Pair<String, int>> parameters;
			for (const ParameterMap::value_type& parameter : parser_state.parameters)
				parameters.emplace_back(parameter.first, parameter.second);
			std::sort(parameters.begin(), parameters.end());

			line += " " + parser_state.name + "(";
			for (const Pair<String, int>& parameter : parameters)
				line += parameter.first + "=" + Rml::ToString(parameter.second) + ",";
			line += ")";
		}
		lines.push_back(std::move(line));
	}

	for (const UniquePtr<ShorthandDefinition>& shorthand : shorthands)
	{
		if (!shorthand)
			continue;

		String line = "s " + shorthand_map->GetName(shorthand->id) + " " + Rml::ToString(static_cast<int>(shorthand->type));
		for (const ShorthandItem& item : shorthand->items)
		{
			if (item.type == ShorthandItemType::Property)
				line += " p:" + property_map->GetName(item.property_id);
			else if (item.type == ShorthandItemType::Shorthand)
				line += " s:" + shorthand_map->GetName(item.shorthand_id);
			line += String(item.optional ? "?" : "") + (item.repeats ? "#" : "");
		}
		lines.push_back(std::move(line));
	}

	for (const UnorderedMap<String, PropertyAlias>::value_type& alias : aliases)
	{
		Vector<Pair<String, String>> keywords;
		for (const UnorderedMap<String, String>::value_type& keyword : alias.second.keyword_map)
			keywords.emplace_back(keyword.first, keyword.second);
		std::sort(keywords.begin(), keywords.end());

		String line = "a " + alias.first + " " + alias.second.target_name;
		for (const Pair<String, String>& keyword : keywords)
			line += " " + keyword.first + "=" + keyword.second;
		lines.push_back(std::move(line));
	}

	std::sort(lines.begin(), lines.end());

	String result;
	StringUtilities::JoinString(result, lines, '\n');
	return result;
}

const PropertyIdSet& PropertySpecification::GetRegisteredProperties() const
{
	return property_ids;
}

const PropertyIdSet& PropertySpecification::GetRegisteredInheritedProperties() const
{
	return property_ids_inherited;
}

const PropertyIdSet& PropertySpecification::GetRegisteredPropertiesForcingLayout() const
{
	return property_ids_forcing_layout;
}

ShorthandId PropertySpecification::RegisterShorthand(const String& shorthand_name, const String& property_names, ShorthandType type, ShorthandId id)
{
	if (id == ShorthandId::Invalid)
		id = shorthand_map->GetOrCreateId(shorthand_name);
	else
		shorthand_map->AddPair(id, shorthand_name);

	StringList property_list;
	StringUtilities::ExpandString(property_list, StringUtilities::ToLower(property_names));

	// Construct the new shorthand definition and resolve its properties.
	UniquePtr<ShorthandDefinition> property_shorthand(new ShorthandDefinition());

	for (const String& raw_name : property_list)
	{
		ShorthandItem item;
		bool optional = false;
		bool repeats = false;
		String name = raw_name;

		if (!raw_name.empty() && raw_name.back() == '?')
		{
			optional = true;
			name.pop_back();
		}
		if (!raw_name.empty() && raw_name.back() == '#')
		{
			repeats = true;
			name.pop_back();
		}

		PropertyId property_id = property_map->GetId(name);
		if (property_id != PropertyId::Invalid)
		{
			// We have a valid property
			if (const PropertyDefinition* property = GetProperty(property_id))
				item = ShorthandItem(property_id, property, optional, repeats);
		}
		else
		{
			// Otherwise, we must be a shorthand
			ShorthandId shorthand_id = shorthand_map->GetId(name);

			// Test for valid shorthand id. The recursive types (and only those) can hold other shorthands.
			if (shorthand_id != ShorthandId::Invalid && (type == ShorthandType::RecursiveRepeat || type == ShorthandType::RecursiveCommaSeparated))
			{
				if (const ShorthandDefinition* shorthand = GetShorthand(shorthand_id))
					item = ShorthandItem(shorthand_id, shorthand, optional, repeats);
			}
		}

		if (item.type == ShorthandItemType::Invalid)
		{
			Log::Message(Log::LT_ERROR, "Shorthand property '%s' was registered with invalid property '%s'.", shorthand_name.c_str(), name.c_str());
			return ShorthandId::Invalid;
		}
		property_shorthand->items.push_back(item);
	}

	property_shorthand->id = id;
	property_shorthand->type = type;
	property_shorthand->inherited = std::any_of(property_shorthand->items.begin(), property_shorthand->items.end(), [](const ShorthandItem& item) {
		return (item.type == ShorthandItemType::Property && item.property_definition->IsInherited()) ||
			(item.type == ShorthandItemType::Shorthand && item.shorthand_definition->inherited);
	});

	const size_t index = (size_t)id;

	if (index >= size_t(ShorthandId::MaxNumIds))
	{
		Log::Message(Log::LT_ERROR, "Error while registering shorthand '%s': Maximum number of allowed shorthands exceeded.", shorthand_name.c_str());
		return ShorthandId::Invalid;
	}

	if (index < shorthands.size())
	{
		// We don't want to overwrite an existing entry.
		if (shorthands[index])
		{
			Log::Message(Log::LT_ERROR, "The shorthand '%s' already exists, ignoring.", shorthand_name.c_str());
			return ShorthandId::Invalid;
		}
	}
	else
	{
		// Resize vector to hold the new index
		shorthands.resize((index * 3) / 2 + 1);
	}

	shorthands[index] = std::move(property_shorthand);
	return id;
}

const ShorthandDefinition* PropertySpecification::GetShorthand(ShorthandId id) const
{
	if (id == ShorthandId::Invalid || (size_t)id >= shorthands.size())
		return nullptr;

	return shorthands[(size_t)id].get();
}

const ShorthandDefinition* PropertySpecification::GetShorthand(const String& shorthand_name) const
{
	return GetShorthand(shorthand_map->GetId(shorthand_name));
}

void PropertySpecification::RegisterAlias(const String& alias_name, const String& target_name, const UnorderedMap<String, String>& keyword_map)
{
	aliases[alias_name] = PropertyAlias{target_name, keyword_map};
}

bool PropertySpecification::ParsePropertyDeclaration(PropertyDictionary& dictionary, const String& property_name, const String& property_value) const
{
	RMLUI_ZoneScoped;

	// Try as a property first
	PropertyId property_id = property_map->GetId(property_name);
	if (property_id != PropertyId::Invalid)
		return ParsePropertyDeclaration(dictionary, property_id, property_value);

	// Then, as a shorthand
	ShorthandId shorthand_id = shorthand_map->GetId(property_name);
	if (shorthand_id != ShorthandId::Invalid)
		return ParseShorthandDeclaration(dictionary, shorthand_id, property_value);

	if (StringUtilities::StartsWith(property_name, "--"))
	{
		Unit unit = {};
		StringList property_values;
		// Allow empty strings for custom properties, otherwise one has to specify an extra pair of quotes for an empty string with the API.
		ParsePropertyResult parse_result =
			(property_value.empty() ? ParsePropertyResult::Success : ParsePropertyValues(property_values, property_value, SplitOption::None));
		switch (parse_result)
		{
		case ParsePropertyResult::Success: unit = Unit::STRING; break;
		case ParsePropertyResult::ContainsVariable: unit = Unit::VAR_EXPRESSION; break;
		case ParsePropertyResult::Error: return false;
		}
		dictionary.SetCustomProperty(property_name, Property{property_value, unit});
		return true;
	}

	auto it_alias = aliases.find(property_name);
	if (it_alias != aliases.end())
	{
		const PropertyAlias& alias = it_alias->second;
		if (alias.target_name.empty() || (property_map->GetId(alias.target_name) == PropertyId::Invalid &&
											  shorthand_map->GetId(alias.target_name) == ShorthandId::Invalid))
			return true;

		if (alias.keyword_map.empty())
			return ParsePropertyDeclaration(dictionary, alias.target_name, property_value);

		StringList items;
		StringUtilities::ExpandString(items, property_value, ',', '(', ')');
		String mapped_value;
		for (const String& item : items)
		{
			auto it_keyword = alias.keyword_map.find(StringUtilities::ToLower(item));
			if (!mapped_value.empty())
				mapped_value += ", ";
			mapped_value += (it_keyword != alias.keyword_map.end() ? it_keyword->second : item);
		}
		return ParsePropertyDeclaration(dictionary, alias.target_name, mapped_value);
	}

	return false;
}

bool PropertySpecification::ParsePropertyDeclaration(PropertyDictionary& dictionary, PropertyId property_id, const String& property_value) const
{
	// Parse as a single property.
	const PropertyDefinition* property_definition = GetProperty(property_id);
	if (!property_definition)
		return false;

	StringList property_values;
	const SplitOption split_option = (property_definition->GetPreserveQuotes() ? SplitOption::PreserveQuotes : SplitOption::None);
	switch (ParsePropertyValues(property_values, property_value, split_option))
	{
	case ParsePropertyResult::Success: break;
	case ParsePropertyResult::ContainsVariable:
	{
		dictionary.SetProperty(property_id, Property{property_value, Unit::VAR_EXPRESSION});
		return true;
	}
	case ParsePropertyResult::Error:
	{
		return false;
	}
	}
	RMLUI_ASSERT(!property_values.empty());

	Property new_property;
	if (!property_definition->ParseValue(new_property, property_values[0]))
		return false;

	dictionary.SetProperty(property_id, new_property);
	return true;
}

bool PropertySpecification::ParseShorthandDeclaration(PropertyDictionary& dictionary, ShorthandId shorthand_id, const String& property_value) const
{
	const ShorthandDefinition* shorthand_definition = GetShorthand(shorthand_id);
	if (!shorthand_definition)
		return false;

	const String css_wide_keyword = StringUtilities::ToLower(StringUtilities::StripWhitespace(property_value));
	if (css_wide_keyword == "inherit" || css_wide_keyword == "initial" || css_wide_keyword == "unset")
	{
		SetShorthandPropertiesToCssWideKeyword(dictionary, shorthand_definition, css_wide_keyword);
		return true;
	}

	const bool is_grid_shorthand = (shorthand_definition->type == ShorthandType::GridLine || shorthand_definition->type == ShorthandType::GridArea ||
		shorthand_definition->type == ShorthandType::GridTemplate || shorthand_definition->type == ShorthandType::Grid);

	SplitOption split_option = SplitOption::Whitespace;
	if (shorthand_definition->type == ShorthandType::RecursiveCommaSeparated)
		split_option = SplitOption::Comma;
	else if (is_grid_shorthand)
		split_option = SplitOption::PreserveQuotes;
	else if (shorthand_definition->type == ShorthandType::BackgroundLayers || shorthand_definition->type == ShorthandType::MaskLayers)
		split_option = SplitOption::None;

	StringList property_values;
	switch (ParsePropertyValues(property_values, property_value, split_option))
	{
	case ParsePropertyResult::Success: break;
	case ParsePropertyResult::ContainsVariable:
	{
		dictionary.SetVarShorthand(shorthand_id, Property{property_value, Unit::VAR_EXPRESSION});
		SetShorthandPropertiesToPendingSubstitution(dictionary, shorthand_definition);
		return true;
	}
	case ParsePropertyResult::Error:
	{
		return false;
	}
	}
	RMLUI_ASSERT(!property_values.empty());

	if (is_grid_shorthand)
		return PropertyParserGrid::ParseShorthand(dictionary, *shorthand_definition, property_values[0]);

	if (shorthand_definition->type == ShorthandType::AnyOrder)
		return ParseAnyOrderShorthand(dictionary, *shorthand_definition, property_values);

	if (shorthand_definition->type == ShorthandType::BackgroundLayers)
		return ParseBackgroundLayersShorthand(dictionary, *shorthand_definition, property_values[0]);

	if (shorthand_definition->type == ShorthandType::MaskLayers)
		return ParseMaskLayersShorthand(dictionary, *shorthand_definition, property_values[0]);

	if (shorthand_definition->type == ShorthandType::Position)
		return ParsePositionShorthand(dictionary, *shorthand_definition, property_values);

	if (shorthand_definition->type == ShorthandType::ScrollbarColor)
		return ParseScrollbarColorShorthand(dictionary, *shorthand_definition, property_values);

	// Handle the special behavior of the flex shorthand first, otherwise it acts like 'FallThrough'.
	if (shorthand_definition->type == ShorthandType::Flex && !property_values.empty())
	{
		RMLUI_ASSERT(shorthand_definition->items.size() == 3);
		if (property_values[0] == "none")
		{
			property_values = {"0", "0", "auto"};
		}
		else
		{
			// Default values when omitted from the 'flex' shorthand is specified here. These defaults are special
			// for this shorthand only, otherwise each underlying property has a different default value.
			const char* default_omitted_values[] = {"1", "1", "0"}; // flex-grow, flex-shrink, flex-basis
			Property new_property;
			bool result = true;
			for (int i = 0; i < 3; i++)
			{
				auto& item = shorthand_definition->items[i];
				result &= item.property_definition->ParseValue(new_property, default_omitted_values[i]);
				dictionary.SetProperty(item.property_id, new_property);
			}
			(void)result;
			RMLUI_ASSERT(result);
		}
	}

	// If this definition is a 'box'-style shorthand (x-top x-right x-bottom x-left) that needs replication.
	if (shorthand_definition->type == ShorthandType::Box && property_values.size() < 4)
	{
		// This array tells which property index each side is parsed from
		Array<int, 4> box_side_to_value_index = {0, 0, 0, 0};
		switch (property_values.size())
		{
		case 1:
			// Only one value is defined, so it is parsed onto all four sides.
			box_side_to_value_index = {0, 0, 0, 0};
			break;
		case 2:
			// Two values are defined, so the first one is parsed onto the top and bottom value, the second onto
			// the left and right.
			box_side_to_value_index = {0, 1, 0, 1};
			break;
		case 3:
			// Three values are defined, so the first is parsed into the top value, the second onto the left and
			// right, and the third onto the bottom.
			box_side_to_value_index = {0, 1, 2, 1};
			break;
		default: RMLUI_ERROR; break;
		}

		for (int i = 0; i < 4; i++)
		{
			RMLUI_ASSERT(shorthand_definition->items[i].type == ShorthandItemType::Property);
			Property new_property;
			int value_index = box_side_to_value_index[i];
			if (!shorthand_definition->items[i].property_definition->ParseValue(new_property, property_values[value_index]))
				return false;

			dictionary.SetProperty(shorthand_definition->items[i].property_definition->GetId(), new_property);
		}
	}
	else if (shorthand_definition->type == ShorthandType::RecursiveRepeat)
	{
		bool result = true;

		for (size_t i = 0; i < shorthand_definition->items.size(); i++)
		{
			const ShorthandItem& item = shorthand_definition->items[i];
			if (item.type == ShorthandItemType::Property)
				result &= ParsePropertyDeclaration(dictionary, item.property_id, property_value);
			else if (item.type == ShorthandItemType::Shorthand)
				result &= ParseShorthandDeclaration(dictionary, item.shorthand_id, property_value);
			else
				result = false;
		}

		if (!result)
			return false;
	}
	else if (shorthand_definition->type == ShorthandType::RecursiveCommaSeparated)
	{
		size_t num_optional = 0;
		for (auto& item : shorthand_definition->items)
			if (item.optional)
				num_optional += 1;

		if (property_values.size() + num_optional < shorthand_definition->items.size())
		{
			// Not enough subvalues declared.
			return false;
		}

		size_t subvalue_i = 0;
		String temp_subvalue;
		for (size_t i = 0; i < shorthand_definition->items.size() && subvalue_i < property_values.size(); i++)
		{
			bool result = false;

			const String* subvalue = &property_values[subvalue_i];

			const ShorthandItem& item = shorthand_definition->items[i];
			if (item.repeats)
			{
				property_values.erase(property_values.begin(), property_values.begin() + subvalue_i);
				temp_subvalue.clear();
				StringUtilities::JoinString(temp_subvalue, property_values);
				subvalue = &temp_subvalue;
			}

			if (item.type == ShorthandItemType::Property)
				result = ParsePropertyDeclaration(dictionary, item.property_id, *subvalue);
			else if (item.type == ShorthandItemType::Shorthand)
				result = ParseShorthandDeclaration(dictionary, item.shorthand_id, *subvalue);

			if (result)
				subvalue_i += 1;
			else if (item.repeats || !item.optional)
				return false;

			if (item.repeats)
				break;
		}
	}
	else
	{
		RMLUI_ASSERT(shorthand_definition->type == ShorthandType::Box || shorthand_definition->type == ShorthandType::FallThrough ||
			shorthand_definition->type == ShorthandType::Replicate || shorthand_definition->type == ShorthandType::Flex);

		// Abort over-specified shorthand values.
		if (property_values.size() > shorthand_definition->items.size())
			return false;

		size_t value_index = 0;
		size_t property_index = 0;

		for (; value_index < property_values.size() && property_index < shorthand_definition->items.size(); property_index++)
		{
			Property new_property;

			if (!shorthand_definition->items[property_index].property_definition->ParseValue(new_property, property_values[value_index]))
			{
				// This definition failed to parse; if we're falling through, try the next property. If there is no
				// next property, then abort!
				if (shorthand_definition->type == ShorthandType::FallThrough || shorthand_definition->type == ShorthandType::Flex)
				{
					if (property_index + 1 < shorthand_definition->items.size())
						continue;
				}
				return false;
			}

			dictionary.SetProperty(shorthand_definition->items[property_index].property_id, new_property);

			// Increment the value index, unless we're replicating the last value and we're up to the last value.
			if (shorthand_definition->type != ShorthandType::Replicate || value_index < property_values.size() - 1)
				value_index++;
		}

		// Abort if we still have values left to parse but no more properties to pass them to.
		if (shorthand_definition->type != ShorthandType::Replicate && value_index < property_values.size() &&
			property_index >= shorthand_definition->items.size())
			return false;
	}

	return true;
}

bool PropertySpecification::ParseAnyOrderShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition,
	const StringList& property_values) const
{
	const size_t num_items = shorthand_definition.items.size();
	Vector<Property> parsed_properties(num_items);
	Vector<bool> item_set(num_items, false);

	for (const String& value : property_values)
	{
		bool parsed = false;
		for (size_t i = 0; i < num_items && !parsed; i++)
		{
			const ShorthandItem& item = shorthand_definition.items[i];
			if (item_set[i] || item.type != ShorthandItemType::Property)
				continue;

			if (item.property_definition->ParseValue(parsed_properties[i], value))
			{
				item_set[i] = true;
				parsed = true;
			}
		}

		if (!parsed)
			return false;
	}

	for (size_t i = 0; i < num_items; i++)
	{
		const ShorthandItem& item = shorthand_definition.items[i];
		if (item.type != ShorthandItemType::Property)
			continue;
		dictionary.SetProperty(item.property_id, item_set[i] ? parsed_properties[i] : *item.property_definition->GetDefaultValue());
	}

	return true;
}

bool PropertySpecification::ParsePositionShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition,
	const StringList& property_values) const
{
	using namespace PropertySpecificationPosition;

	if (shorthand_definition.items.size() != 2 || shorthand_definition.items[0].type != ShorthandItemType::Property ||
		shorthand_definition.items[1].type != ShorthandItemType::Property)
		return false;

	const size_t num_values = property_values.size();
	PositionToken tokens[4];
	if (num_values != 1 && num_values != 2 && num_values != 4)
		return false;
	for (size_t i = 0; i < num_values; i++)
		tokens[i] = ClassifyPositionToken(property_values[i]);

	String value_x, value_y;
	if (num_values == 1)
	{
		const PositionToken& token = tokens[0];
		const bool vertical = (token.keyword && token.axis == PositionAxis::Vertical);
		value_x = (vertical ? "50%" : token.value);
		value_y = (vertical ? token.value : "50%");
	}
	else if (num_values == 2)
	{
		PositionToken first = tokens[0];
		PositionToken second = tokens[1];
		if (first.keyword && second.keyword && (first.axis == PositionAxis::Vertical || second.axis == PositionAxis::Horizontal))
			std::swap(first, second);
		if ((first.keyword && first.axis == PositionAxis::Vertical) || (second.keyword && second.axis == PositionAxis::Horizontal))
			return false;
		value_x = first.value;
		value_y = second.value;
	}
	else
	{
		for (size_t pair = 0; pair < 2; pair++)
		{
			const PositionToken& edge = tokens[2 * pair];
			const PositionToken& offset = tokens[2 * pair + 1];
			if (!edge.keyword || edge.axis == PositionAxis::Either || offset.keyword)
				return false;

			String& value = (edge.axis == PositionAxis::Horizontal ? value_x : value_y);
			if (!value.empty())
				return false;
			value = (edge.far_edge ? "calc(100% - " + offset.value + ")" : offset.value);
		}
	}

	Property property_x, property_y;
	if (!shorthand_definition.items[0].property_definition->ParseValue(property_x, value_x) ||
		!shorthand_definition.items[1].property_definition->ParseValue(property_y, value_y))
		return false;

	dictionary.SetProperty(shorthand_definition.items[0].property_id, property_x);
	dictionary.SetProperty(shorthand_definition.items[1].property_id, property_y);
	return true;
}

bool PropertySpecification::ParseScrollbarColorShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition,
	const StringList& property_values) const
{
	if (shorthand_definition.items.size() != 2 || shorthand_definition.items[0].type != ShorthandItemType::Property ||
		shorthand_definition.items[1].type != ShorthandItemType::Property)
		return false;

	const bool is_auto = (property_values.size() == 1 && StringUtilities::ToLower(property_values[0]) == "auto");
	if (!is_auto && property_values.size() != 2)
		return false;

	Property properties[2];
	for (size_t i = 0; i < 2; i++)
	{
		const String& value = (is_auto ? property_values[0] : property_values[i]);
		const PropertyDefinition* definition = shorthand_definition.items[i].property_definition;
		if (!definition->ParseValue(properties[i], value))
			return false;
		Property current_color;
		const bool is_current_color = (properties[i].unit == Unit::KEYWORD && definition->ParseValue(current_color, "currentcolor") &&
			current_color.unit == Unit::KEYWORD && current_color.Get<int>() == properties[i].Get<int>());
		if (!is_auto && properties[i].unit != Unit::COLOUR && !is_current_color)
			return false;
	}

	for (size_t i = 0; i < 2; i++)
		dictionary.SetProperty(shorthand_definition.items[i].property_id, properties[i]);
	return true;
}

bool PropertySpecification::ParseBackgroundLayersShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition,
	const String& property_value) const
{
	if (shorthand_definition.items.empty() || shorthand_definition.items[0].type != ShorthandItemType::Property)
		return false;

	const ShorthandItem& decorator_item = shorthand_definition.items[0];
	const ShorthandItem* color_item = nullptr;
	if (shorthand_definition.items.size() > 1 && shorthand_definition.items[1].type == ShorthandItemType::Property)
		color_item = &shorthand_definition.items[1];
	const ShorthandItem* origin_item = nullptr;
	const ShorthandItem* clip_item = nullptr;
	if (shorthand_definition.items.size() == 4 && shorthand_definition.items[2].type == ShorthandItemType::Property &&
		shorthand_definition.items[3].type == ShorthandItemType::Property)
	{
		origin_item = &shorthand_definition.items[2];
		clip_item = &shorthand_definition.items[3];
	}

	String normalized_value = property_value;
	for (char& c : normalized_value)
	{
		if (StringUtilities::IsWhitespace(c))
			c = ' ';
	}

	StringList layers;
	StringUtilities::ExpandString(layers, normalized_value, ',', '(', ')');

	Property color_property;
	bool has_color = false;
	String decorator_value;
	String origin_value;
	String clip_value;

	for (size_t layer_index = 0; layer_index < layers.size(); layer_index++)
	{
		const bool final_layer = (layer_index + 1 == layers.size());

		StringList tokens;
		StringUtilities::ExpandString(tokens, layers[layer_index], ' ', '(', ')', true);

		String layer;
		StringList boxes;
		for (const String& token : tokens)
		{
			if (token.empty())
				continue;

			if (color_item && final_layer && !has_color && color_item->property_definition->ParseValue(color_property, token))
			{
				has_color = true;
				continue;
			}

			const String lower = StringUtilities::ToLower(token);
			if (origin_item && (lower == "border-box" || lower == "padding-box" || lower == "content-box"))
			{
				if (boxes.size() == 2)
					return false;
				boxes.push_back(lower);
				continue;
			}

			if (!layer.empty())
				layer += ' ';
			layer += token;
		}

		if (layer.empty())
		{
			if (!final_layer || (!has_color && boxes.empty()))
				return false;
			layer = "none";
		}

		if (layer_index > 0)
		{
			decorator_value += ", ";
			origin_value += ", ";
			clip_value += ", ";
		}
		decorator_value += layer;
		origin_value += (boxes.empty() ? String("padding-box") : boxes[0]);
		clip_value += (boxes.empty() ? String("border-box") : boxes.back());
	}

	Property decorator_property;
	if (!decorator_item.property_definition->ParseValue(decorator_property, decorator_value))
		return false;

	Property origin_property;
	Property clip_property;
	if (origin_item &&
		(!origin_item->property_definition->ParseValue(origin_property, origin_value) ||
			!clip_item->property_definition->ParseValue(clip_property, clip_value)))
		return false;

	dictionary.SetProperty(decorator_item.property_id, decorator_property);

	if (color_item)
		dictionary.SetProperty(color_item->property_id, has_color ? color_property : *color_item->property_definition->GetDefaultValue());

	if (origin_item)
	{
		dictionary.SetProperty(origin_item->property_id, origin_property);
		dictionary.SetProperty(clip_item->property_id, clip_property);
	}

	return true;
}

bool PropertySpecification::ParseMaskLayersShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& shorthand_definition,
	const String& property_value) const
{
	if (shorthand_definition.items.size() != 4)
		return false;
	for (const ShorthandItem& item : shorthand_definition.items)
	{
		if (item.type != ShorthandItemType::Property)
			return false;
	}

	String normalized_value = property_value;
	for (char& c : normalized_value)
	{
		if (StringUtilities::IsWhitespace(c))
			c = ' ';
	}

	StringList layers;
	StringUtilities::ExpandString(layers, normalized_value, ',', '(', ')');
	if (layers.empty())
		return false;

	String values[4];
	for (const String& layer_value : layers)
	{
		StringList tokens;
		StringUtilities::ExpandString(tokens, layer_value, ' ', '(', ')', true);

		String image;
		String composite;
		StringList boxes;
		for (const String& token : tokens)
		{
			if (token.empty())
				continue;

			const String lower = StringUtilities::ToLower(token);
			if (lower == "add" || lower == "subtract" || lower == "intersect" || lower == "exclude")
			{
				if (!composite.empty())
					return false;
				composite = lower;
				continue;
			}
			if (lower == "border-box" || lower == "padding-box" || lower == "content-box")
			{
				if (boxes.size() == 2)
					return false;
				boxes.push_back(lower);
				continue;
			}

			if (!image.empty())
				image += ' ';
			image += token;
		}

		if (image.empty() || StringUtilities::ToLower(image) == "none")
		{
			if (layers.size() != 1 || !composite.empty() || !boxes.empty())
				return false;
			image = "none";
		}

		const String origin = (boxes.empty() ? String("border-box") : boxes[0]);
		const String clip = (boxes.size() == 2 ? boxes[1] : origin);
		const String layer_values[4] = {image, composite.empty() ? String("add") : composite, origin, clip};
		for (int i = 0; i < 4; i++)
		{
			if (!values[i].empty())
				values[i] += ", ";
			values[i] += layer_values[i];
		}
	}

	Property properties[4];
	for (int i = 0; i < 4; i++)
	{
		if (!shorthand_definition.items[i].property_definition->ParseValue(properties[i], values[i]))
			return false;
	}
	for (int i = 0; i < 4; i++)
		dictionary.SetProperty(shorthand_definition.items[i].property_id, properties[i]);

	return true;
}

void PropertySpecification::SetPropertyDefaults(PropertyDictionary& dictionary) const
{
	for (const auto& property : properties)
	{
		if (property && dictionary.GetProperty(property->GetId()) == nullptr)
			dictionary.SetProperty(property->GetId(), *property->GetDefaultValue());
	}
}

String PropertySpecification::PropertiesToString(const PropertyDictionary& dictionary, bool include_name, char delimiter) const
{
	const PropertyMap& properties = dictionary.GetProperties();

	// For determinism we print the strings in order of increasing property ids.
	Vector<PropertyId> ids;
	ids.reserve(properties.size());
	for (auto& pair : properties)
		ids.push_back(pair.first);

	std::sort(ids.begin(), ids.end());

	String result;
	for (PropertyId id : ids)
	{
		const Property& p = properties.find(id)->second;
		if (include_name)
			result += property_map->GetName(id) + ": ";
		result += p.ToString() + delimiter;
	}

	if (!result.empty())
		result.pop_back();

	return result;
}

PropertySpecification::ParsePropertyResult PropertySpecification::ParsePropertyValues(StringList& values_list, const String& values,
	const SplitOption split_option) const
{
	RMLUI_ASSERT(values_list.empty());

	const bool split_by_comma = (split_option == SplitOption::Comma);
	const bool split_by_whitespace = (split_option == SplitOption::Whitespace);
	const bool split_values = (split_by_comma || split_by_whitespace);
	const bool keep_quotes = (split_by_comma || split_option == SplitOption::PreserveQuotes);

	String value;

	auto SubmitExactValue = [&]() {
		values_list.push_back(std::move(value));
		value.clear();
	};

	auto SubmitValue = [&]() {
		value = StringUtilities::StripWhitespace(value);
		if (!value.empty())
			SubmitExactValue();
	};

	auto IsAllWhitespace = [](const String& string) { return std::all_of(string.begin(), string.end(), StringUtilities::IsWhitespace); };
	auto IsIdentifierChar = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'; };
	auto EndsWithVariable = [&](const String& string) {
		return StringUtilities::EndsWith(string, "var") && (string.size() == 3 || !IsIdentifierChar(string[string.size() - 4]));
	};

	auto Error = [&]() {
		values_list.clear();
		return ParsePropertyResult::Error;
	};
	auto ContainsVariable = [&]() {
		values_list.clear();
		return ParsePropertyResult::ContainsVariable;
	};

	enum ParseState { VALUE, VALUE_PARENTHESIS, VALUE_QUOTE, VALUE_QUOTE_ESCAPE_NEXT };
	ParseState state = VALUE;
	int open_parentheses = 0;
	char open_quote_character = 0;
	size_t character_index = 0;

	while (character_index < values.size())
	{
		const char character = values[character_index];
		character_index++;

		switch (state)
		{
		case VALUE:
		{
			if (character == ';')
			{
				if (value.size() > 0)
				{
					values_list.push_back(value);
					value.clear();
				}
			}
			else if ((split_by_comma && character == ',') || (split_by_whitespace && StringUtilities::IsWhitespace(character)))
			{
				SubmitValue();
			}
			else if (character == '"' || character == '\'')
			{
				state = VALUE_QUOTE;
				open_quote_character = character;
				if (split_by_whitespace)
					SubmitValue();
				else if (keep_quotes)
					value += character;
				else if (IsAllWhitespace(value))
					value.clear();
				else
					return Error();
			}
			else if (character == '(')
			{
				if (EndsWithVariable(value))
					return ContainsVariable();
				open_parentheses = 1;
				value += character;
				state = VALUE_PARENTHESIS;
			}
			else
			{
				value += character;
			}
		}
		break;
		case VALUE_PARENTHESIS:
		{
			if (character == '(')
			{
				if (EndsWithVariable(value))
					return ContainsVariable();
				open_parentheses++;
			}
			else if (character == ')')
			{
				open_parentheses--;
				if (open_parentheses == 0)
					state = VALUE;
			}
			else if (character == '"' || character == '\'')
			{
				state = VALUE_QUOTE;
				open_quote_character = character;
			}

			value += character;
		}
		break;
		case VALUE_QUOTE:
		{
			if (character == open_quote_character)
			{
				if (open_parentheses == 0)
				{
					state = VALUE;
					if (keep_quotes)
						value += character;
					else
						SubmitExactValue();
				}
				else
				{
					state = VALUE_PARENTHESIS;
					value += character;
				}
			}
			else if (character == '\\')
			{
				state = VALUE_QUOTE_ESCAPE_NEXT;
			}
			else
			{
				value += character;
			}
		}
		break;
		case VALUE_QUOTE_ESCAPE_NEXT:
		{
			if (split_option != SplitOption::PreserveQuotes && (character == '"' || character == '\'' || character == '\\'))
			{
				value += character;
			}
			else
			{
				value += '\\';
				value += character;
			}
			state = VALUE_QUOTE;
		}
		break;
		}
	}

	if (state == VALUE)
		SubmitValue();

	if (!split_values && values_list.size() > 1)
		return Error();

	if (values_list.empty())
		return ParsePropertyResult::Error;

	return ParsePropertyResult::Success;
}

} // namespace Rml
