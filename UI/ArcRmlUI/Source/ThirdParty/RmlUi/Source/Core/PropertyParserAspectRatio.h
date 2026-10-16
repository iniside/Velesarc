#pragma once

#include "../../Include/RmlUi/Core/PropertyParser.h"
#include "PropertyParserRatio.h"

namespace Rml {

/**
    Parses the 'aspect-ratio' property: 'auto', '<ratio>', or 'auto && <ratio>' in either order, where '<ratio>' is '<number>' or
    '<number> / <number>'. The value is a Vector2f holding the
    ratio (width over height, zero when absent) and one when 'auto' was specified.
 */
class PropertyParserAspectRatio : public PropertyParser {
public:
	PropertyParserAspectRatio();
	virtual ~PropertyParserAspectRatio();

	bool ParseValue(Property& property, const String& value, const ParameterMap& parameters) const override;

private:
	PropertyParserRatio ratio_parser;
};

} // namespace Rml
