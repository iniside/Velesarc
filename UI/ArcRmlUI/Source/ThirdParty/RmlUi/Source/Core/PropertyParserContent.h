#pragma once

#include "../../Include/RmlUi/Core/PropertyParser.h"

namespace Rml {

class Element;

/**
    A property parser for the 'content' property of the ::before and ::after pseudo-elements: 'none', 'normal', or a sequence of strings and
    attr() functions.
 */

class PropertyParserContent : public PropertyParser {
public:
	bool ParseValue(Property& property, const String& value, const ParameterMap& parameters) const override;

	/// Returns true if a parsed content value generates a box, false for 'none' and 'normal'.
	static bool GeneratesBox(const String& value);
	/// Returns the text of a parsed content value, reading attr() values from the given element.
	static String ResolveText(const String& value, const Element* element);
	/// Returns true if a parsed content value reads any of the given attributes through attr().
	static bool ReadsAnyAttribute(const String& value, const ElementAttributes& attributes);
};

} // namespace Rml
