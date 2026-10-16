#include "PropertyParserTextShadow.h"
#include "../../Include/RmlUi/Core/DecorationTypes.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"

namespace Rml {

PropertyParserTextShadow::PropertyParserTextShadow(PropertyParser* parser_color, PropertyParser* parser_length) :
	parser_color(parser_color), parser_length(parser_length)
{
	RMLUI_ASSERT(parser_color && parser_length);
}

bool PropertyParserTextShadow::ParseValue(Property& property, const String& value, const ParameterMap& parameters) const
{
	const bool single = (parameters.count("single") != 0);
	if (value.empty() || value == "none")
	{
		if (single)
			return false;
		property.unit = Unit::BOXSHADOWLIST;
		property.value = Variant(BoxShadowList{});
		return true;
	}

	StringList shadows_string_list;
	{
		const String lowercase_value = StringUtilities::ToLower(value);
		StringUtilities::ExpandString(shadows_string_list, lowercase_value, ',', '(', ')');
	}

	if (shadows_string_list.empty())
		return false;

	if (shadows_string_list.size() > 1 && single)
		return false;

	const ParameterMap empty_parameter_map;

	BoxShadowList shadow_list;
	shadow_list.reserve(shadows_string_list.size());

	for (const String& shadow_str : shadows_string_list)
	{
		StringList arguments;
		StringUtilities::ExpandString(arguments, shadow_str, ' ', '(', ')');

		BoxShadow shadow;
		shadow.current_color = true;
		int length_count = 0;
		bool has_color = false;
		bool color_after_lengths = false;

		for (const String& argument : arguments)
		{
			if (argument.empty())
				continue;

			Property prop;
			if (parser_length->ParseValue(prop, argument, empty_parameter_map))
			{
				if (color_after_lengths)
					return false;
				switch (length_count)
				{
				case 0: shadow.offset_x = prop.GetNumericValue(); break;
				case 1: shadow.offset_y = prop.GetNumericValue(); break;
				case 2:
					shadow.blur_radius = prop.GetNumericValue();
					if (shadow.blur_radius.number < 0.f)
						return false;
					break;
				default: return false;
				}
				length_count += 1;
			}
			else if (has_color || length_count == 1)
			{
				return false;
			}
			else if (argument == "currentcolor")
			{
				has_color = true;
				color_after_lengths = (length_count > 0);
			}
			else if (parser_color->ParseValue(prop, argument, empty_parameter_map))
			{
				shadow.color = prop.Get<Colourb>().ToPremultiplied();
				shadow.current_color = false;
				has_color = true;
				color_after_lengths = (length_count > 0);
			}
			else
			{
				return false;
			}
		}

		if (length_count < 2)
			return false;

		shadow_list.push_back(shadow);
	}

	property.unit = Unit::BOXSHADOWLIST;
	property.value = Variant(std::move(shadow_list));

	return true;
}

} // namespace Rml
