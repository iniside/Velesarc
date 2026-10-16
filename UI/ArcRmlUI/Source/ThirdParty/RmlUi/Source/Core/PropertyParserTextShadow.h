#pragma once

#include "../../Include/RmlUi/Core/PropertyParser.h"

namespace Rml {

/**
    Parses the RCSS 'text-shadow' property: 'none' or a comma-separated list of '<color>? && <length>{2,3}'. With the 'single' parameter, as
    used by the 'drop-shadow()' filter, exactly one shadow is required: 'none', an empty value and lists are rejected.
*/

class PropertyParserTextShadow : public PropertyParser {
public:
	PropertyParserTextShadow(PropertyParser* parser_color, PropertyParser* parser_length);

	bool ParseValue(Property& property, const String& value, const ParameterMap& parameters) const override;

private:
	PropertyParser* parser_color;
	PropertyParser* parser_length;
};

} // namespace Rml
