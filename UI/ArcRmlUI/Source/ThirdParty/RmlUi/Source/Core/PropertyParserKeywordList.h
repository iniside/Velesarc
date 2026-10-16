#pragma once

#include "../../Include/RmlUi/Core/PropertyParser.h"

namespace Rml {

/**
    A property parser for a comma-separated list of keywords, each from the specified list. The value is stored as a string of the
    lower-case keywords separated by ", ".
 */

class PropertyParserKeywordList : public PropertyParser {
public:
	bool ParseValue(Property& property, const String& value, const ParameterMap& parameters) const override;
};

} // namespace Rml
