#include "PropertyParserDecorator.h"
#include "../../Include/RmlUi/Core/Decorator.h"
#include "../../Include/RmlUi/Core/Factory.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/PropertySpecification.h"
#include "../../Include/RmlUi/Core/StyleSheetTypes.h"
#include "../../Include/RmlUi/Core/TypeConverter.h"
#include "PropertyParserNumber.h"

namespace Rml {

struct PropertyParserDecoratorData {
	const PropertyParserNumber length_percent = PropertyParserNumber(Unit::LENGTH_PERCENT, Unit::PX);
};

ControlledLifetimeResource<PropertyParserDecoratorData> PropertyParserDecorator::parser_data;

void PropertyParserDecorator::Initialize()
{
	parser_data.Initialize();
}
void PropertyParserDecorator::Shutdown()
{
	parser_data.Shutdown();
}

PropertyParserDecorator::PropertyParserDecorator() {}

PropertyParserDecorator::~PropertyParserDecorator() {}

static bool IsLayerDecoratorType(const String& type)
{
	return type == "linear-gradient" || type == "repeating-linear-gradient" || type == "radial-gradient" || type == "repeating-radial-gradient" ||
		type == "conic-gradient" || type == "repeating-conic-gradient";
}

static StringList TokenizeDecoratorLayer(const String& value)
{
	StringList tokens;
	String token;
	int depth = 0;
	char quote = 0;

	auto Submit = [&]() {
		if (!token.empty())
			tokens.push_back(std::move(token));
		token.clear();
	};

	for (const char c : value)
	{
		if (quote)
		{
			if (c == quote)
				quote = 0;
			token += c;
		}
		else if (c == '"' || c == '\'')
		{
			quote = c;
			token += c;
		}
		else if (c == '(')
		{
			depth++;
			token += c;
		}
		else if (c == ')')
		{
			depth--;
			token += c;
		}
		else if (depth == 0 && StringUtilities::IsWhitespace(c))
		{
			Submit();
		}
		else if (depth == 0 && c == '/')
		{
			Submit();
			tokens.push_back("/");
		}
		else
		{
			token += c;
		}
	}
	Submit();

	return tokens;
}

bool PropertyParserDecorator::ParseLayerPositionComponent(const String& token, NumericValue& out_value, int& out_axis) const
{
	const String lower = StringUtilities::ToLower(token);
	if (lower == "left" || lower == "right")
	{
		out_value = NumericValue(lower == "left" ? 0.f : 100.f, Unit::PERCENT);
		out_axis = 0;
		return true;
	}
	if (lower == "top" || lower == "bottom")
	{
		out_value = NumericValue(lower == "top" ? 0.f : 100.f, Unit::PERCENT);
		out_axis = 1;
		return true;
	}
	if (lower == "center")
	{
		out_value = NumericValue(50.f, Unit::PERCENT);
		out_axis = -1;
		return true;
	}

	Property property;
	if (!parser_data->length_percent.ParseValue(property, token, ParameterMap()))
		return false;
	out_value = property.GetNumericValue();
	out_axis = 2;
	return true;
}

bool PropertyParserDecorator::ParseLayerPosition(const StringList& tokens, NumericValue& out_x, NumericValue& out_y) const
{
	NumericValue values[2];
	int axes[2] = {};
	for (size_t i = 0; i < tokens.size(); i++)
	{
		if (!ParseLayerPositionComponent(tokens[i], values[i], axes[i]))
			return false;
	}

	if (tokens.size() == 1)
	{
		const bool vertical = (axes[0] == 1);
		out_x = (vertical ? NumericValue(50.f, Unit::PERCENT) : values[0]);
		out_y = (vertical ? values[0] : NumericValue(50.f, Unit::PERCENT));
		return true;
	}

	if (tokens.size() != 2)
		return false;

	const bool swapped = (axes[0] == 1 || axes[1] == 0);
	const int axis_x = (swapped ? axes[1] : axes[0]);
	const int axis_y = (swapped ? axes[0] : axes[1]);
	if (axis_x == 1 || axis_y == 0)
		return false;

	out_x = (swapped ? values[1] : values[0]);
	out_y = (swapped ? values[0] : values[1]);
	return true;
}

bool PropertyParserDecorator::ParseValue(Property& property, const String& decorator_string_value, const ParameterMap& /*parameters*/) const
{
	// Decorators are declared as
	//   decorator: <decorator-value>[, <decorator-value> ...];
	// Where <decorator-value> is either a @decorator name:
	//   decorator: invader-theme-background, ...;
	// or is an anonymous decorator with inline properties
	//   decorator: tiled-box( <shorthand properties> ), ...;
	// Gradient decorators and url() images additionally accept the CSS background layer syntax:
	//   <position> [ / <size> ]? || <repeat> || <attachment>
	// A 'none' item in a list is an empty layer, which keeps the layer indices of the background and mask layer longhands.

	if (decorator_string_value.empty() || decorator_string_value == "none")
	{
		property.value = Variant(DecoratorsPtr());
		property.unit = Unit::DECORATOR;
		return true;
	}

	RMLUI_ZoneScoped;

	// Make sure we don't split inside the parenthesis since they may appear in decorator shorthands.
	StringList decorator_string_list;
	StringUtilities::ExpandString(decorator_string_list, decorator_string_value, ',', '(', ')');

	DecoratorDeclarationList decorators;
	decorators.value = decorator_string_value;
	decorators.list.reserve(decorator_string_list.size());

	// Get or instance each decorator in the comma-separated string list
	for (const String& decorator_string : decorator_string_list)
	{
		if (StringUtilities::ToLower(StringUtilities::StripWhitespace(decorator_string)) == "none")
		{
			decorators.list.emplace_back(DecoratorDeclaration{String(), nullptr, {}, {}});
			continue;
		}

		const StringList tokens = TokenizeDecoratorLayer(decorator_string);

		String name;
		String type;
		String shorthand;

		DecoratorLayer layer;
		bool has_layer_tokens = false;
		bool has_position = false;
		bool has_repeat = false;

		for (size_t i = 0; i < tokens.size(); i++)
		{
			const String& token = tokens[i];
			const String lower = StringUtilities::ToLower(token);

			const size_t function_open = token.find('(');
			if (function_open != String::npos && function_open > 0 && token.back() == ')')
			{
				if (!type.empty() || !name.empty())
					return false;
				type = StringUtilities::StripWhitespace(token.substr(0, function_open));
				shorthand = token.substr(function_open + 1, token.size() - function_open - 2);
				continue;
			}

			if (lower == "repeat" || lower == "no-repeat" || lower == "space" || lower == "round" || lower == "repeat-x" || lower == "repeat-y")
			{
				if (has_repeat)
					return false;
				has_repeat = true;
				has_layer_tokens = true;

				if (lower == "repeat-x" || lower == "repeat-y")
				{
					layer.repeat_x = (lower == "repeat-x");
					layer.repeat_y = (lower == "repeat-y");
					continue;
				}

				layer.repeat_x = (lower != "no-repeat");
				layer.repeat_y = layer.repeat_x;
				if (i + 1 < tokens.size())
				{
					const String next = StringUtilities::ToLower(tokens[i + 1]);
					if (next == "repeat" || next == "no-repeat" || next == "space" || next == "round")
					{
						layer.repeat_y = (next != "no-repeat");
						i++;
					}
				}
				continue;
			}

			if (lower == "scroll" || lower == "fixed" || lower == "local")
			{
				has_layer_tokens = true;
				continue;
			}

			NumericValue position_component;
			int position_axis = 0;
			if (!has_position && ParseLayerPositionComponent(token, position_component, position_axis))
			{
				StringList position_tokens;
				while (i < tokens.size() && position_tokens.size() < 3 && ParseLayerPositionComponent(tokens[i], position_component, position_axis))
					position_tokens.push_back(tokens[i++]);

				if (!ParseLayerPosition(position_tokens, layer.position_x, layer.position_y))
					return false;
				has_position = true;
				has_layer_tokens = true;

				if (i < tokens.size() && tokens[i] == "/")
				{
					StringList size_tokens;
					for (i++; i < tokens.size() && size_tokens.size() < 2; i++)
					{
						const String size_lower = StringUtilities::ToLower(tokens[i]);
						Property size_property;
						if (size_lower != "auto" && size_lower != "cover" && size_lower != "contain" &&
							!parser_data->length_percent.ParseValue(size_property, tokens[i], ParameterMap()))
							break;
						size_tokens.push_back(size_lower);
					}

					if (size_tokens.empty())
						return false;

					if (size_tokens[0] == "cover" || size_tokens[0] == "contain")
					{
						if (size_tokens.size() != 1)
							return false;
						layer.size_type = (size_tokens[0] == "cover" ? DecoratorLayer::SizeType::Cover : DecoratorLayer::SizeType::Contain);
					}
					else
					{
						NumericValue sizes[2] = {NumericValue(0.f, Unit::KEYWORD), NumericValue(0.f, Unit::KEYWORD)};
						for (size_t size_index = 0; size_index < size_tokens.size(); size_index++)
						{
							if (size_tokens[size_index] == "cover" || size_tokens[size_index] == "contain")
								return false;
							Property size_property;
							if (size_tokens[size_index] != "auto" && parser_data->length_percent.ParseValue(size_property, size_tokens[size_index], ParameterMap()))
								sizes[size_index] = size_property.GetNumericValue();
						}
						layer.size_x = sizes[0];
						layer.size_y = sizes[1];
						const bool both_auto = (sizes[0].unit == Unit::KEYWORD && sizes[1].unit == Unit::KEYWORD);
						layer.size_type = (both_auto ? DecoratorLayer::SizeType::Auto : DecoratorLayer::SizeType::Explicit);
					}
				}
				i--;
				continue;
			}

			if (token == "/")
				return false;

			if (i == 0 && type.empty())
			{
				name = token;
				continue;
			}

			return false; // Bail out if we have an invalid keyword.
		}

		const String type_lower = StringUtilities::ToLower(type);

		if (type.empty())
		{
			if (name.empty() || has_layer_tokens)
				return false;

			// We found no parenthesis, that means the value must be a name of a @decorator rule.
			decorators.list.emplace_back(DecoratorDeclaration{name, nullptr, {}, {}});
		}
		else if (type_lower == "url")
		{
			String source = StringUtilities::StripWhitespace(shorthand);
			if (source.size() >= 2 && (source.front() == '"' || source.front() == '\'') && source.back() == source.front())
				source = source.substr(1, source.size() - 2);

			String fit;
			switch (layer.size_type)
			{
			case DecoratorLayer::SizeType::Cover: fit = "cover"; break;
			case DecoratorLayer::SizeType::Contain: fit = "contain"; break;
			case DecoratorLayer::SizeType::Auto:
				fit = (layer.repeat_x && layer.repeat_y ? "repeat" : (layer.repeat_x ? "repeat-x" : (layer.repeat_y ? "repeat-y" : "scale-none")));
				break;
			case DecoratorLayer::SizeType::Explicit:
				if (layer.size_x != NumericValue(100.f, Unit::PERCENT) || layer.size_y != NumericValue(100.f, Unit::PERCENT))
				{
					Log::Message(Log::LT_WARNING, "Background size '%s' is not supported for url() layers, use 100%% 100%%, auto, cover or contain.",
						decorator_string.c_str());
					return false;
				}
				fit = "fill";
				break;
			}

			DecoratorInstancer* instancer = Factory::GetDecoratorInstancer("image");
			if (!instancer)
				return false;

			String unit_x, unit_y;
			if (!TypeConverter<Unit, String>::Convert(layer.position_x.unit, unit_x) || !TypeConverter<Unit, String>::Convert(layer.position_y.unit, unit_y))
				return false;
			const String align_x = ToString(layer.position_x.number) + unit_x;
			const String align_y = ToString(layer.position_y.number) + unit_y;

			const String image_shorthand = "\"" + source + "\" none " + fit + " " + align_x + " " + align_y;
			const PropertySpecification& specification = instancer->GetPropertySpecification();
			PropertyDictionary properties;
			if (!specification.ParsePropertyDeclaration(properties, "decorator", image_shorthand))
				return false;
			specification.SetPropertyDefaults(properties);

			decorators.list.emplace_back(DecoratorDeclaration{"image", instancer, std::move(properties), {}});
		}
		else
		{
			if (has_layer_tokens && !IsLayerDecoratorType(type_lower))
				return false;

			// Since we have parentheses it must be an anonymous decorator with inline properties
			// Check for valid decorator type
			DecoratorInstancer* instancer = Factory::GetDecoratorInstancer(type);
			if (!instancer)
			{
				Log::Message(Log::LT_WARNING, "Decorator type '%s' not found.", type.c_str());
				return false;
			}

			const PropertySpecification& specification = instancer->GetPropertySpecification();

			// Parse the shorthand properties given by the 'decorator' shorthand property
			PropertyDictionary properties;
			if (!specification.ParsePropertyDeclaration(properties, "decorator", shorthand))
			{
				// Empty values are allowed in decorators, if the value is not empty we must have encountered a parser error.
				if (!StringUtilities::StripWhitespace(shorthand).empty())
					return false;
			}

			// Set unspecified values to their defaults
			specification.SetPropertyDefaults(properties);

			decorators.list.emplace_back(DecoratorDeclaration{type, instancer, std::move(properties), layer});
		}
	}

	if (decorators.list.empty())
		return false;

	property.value = Variant(MakeShared<DecoratorDeclarationList>(std::move(decorators)));
	property.unit = Unit::DECORATOR;

	return true;
}

} // namespace Rml
