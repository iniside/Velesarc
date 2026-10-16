#pragma once

#include "../../Include/RmlUi/Core/PropertyParser.h"

namespace Rml {

class PropertyDictionary;
struct ShorthandDefinition;

/**
    Parses the RCSS grid properties. The parser parameter selects the value grammar: 'track-list', 'auto-tracks', 'areas', 'placement'
    or 'auto-flow'.
*/

class PropertyParserGrid : public PropertyParser {
public:
	PropertyParserGrid(PropertyParser* parser_length_percent);

	bool ParseValue(Property& property, const String& value, const ParameterMap& parameters) const override;

	/// Expands the 'grid-row', 'grid-column', 'grid-area', 'grid-template' and 'grid' shorthands into their longhand properties.
	/// @return True if every longhand parsed, in which case all of them have been set on the dictionary.
	static bool ParseShorthand(PropertyDictionary& dictionary, const ShorthandDefinition& definition, const String& value);

private:
	PropertyParser* parser_length_percent;
};

} // namespace Rml
