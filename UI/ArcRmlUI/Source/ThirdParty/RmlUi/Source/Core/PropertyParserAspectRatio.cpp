#include "PropertyParserAspectRatio.h"

namespace Rml {

PropertyParserAspectRatio::PropertyParserAspectRatio() {}

PropertyParserAspectRatio::~PropertyParserAspectRatio() {}

bool PropertyParserAspectRatio::ParseValue(Property& property, const String& value, const ParameterMap& parameters) const
{
	StringList words;
	StringUtilities::ExpandString(words, StringUtilities::ToLower(value), ' ', true);

	bool has_auto = false;
	String ratio_text;
	for (const String& word : words)
	{
		if (word == "auto")
		{
			if (has_auto)
				return false;
			has_auto = true;
		}
		else
		{
			if (!ratio_text.empty())
				ratio_text += ' ';
			ratio_text += word;
		}
	}

	float ratio = 0.f;
	if (!ratio_text.empty())
	{
		Vector2f terms(0.f, 1.f);
		if (ratio_text.find('/') == String::npos)
		{
			if (!TypeConverter<String, float>::Convert(ratio_text, terms.x))
				return false;
		}
		else
		{
			Property ratio_property;
			if (!ratio_parser.ParseValue(ratio_property, ratio_text, parameters))
				return false;
			terms = ratio_property.value.Get<Vector2f>();
		}

		if (terms.x <= 0.f || terms.y <= 0.f)
			return false;
		ratio = terms.x / terms.y;
	}
	else if (!has_auto)
	{
		return false;
	}

	property.value = Variant(Vector2f(ratio, has_auto ? 1.f : 0.f));
	property.unit = Unit::RATIO;
	return true;
}

} // namespace Rml
