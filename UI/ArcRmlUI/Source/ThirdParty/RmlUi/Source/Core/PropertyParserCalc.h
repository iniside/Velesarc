#pragma once

#include "../../Include/RmlUi/Core/PropertyParser.h"

namespace Rml {

/**
    A property parser for the math functions calc(), min(), max() and clamp() over lengths, percentages and numbers.
    Expressions using a single unit are folded into a plain numeric value, others are stored as a CalcExpressionPtr with unit CALC.
 */

class PropertyParserCalc : public PropertyParser {
public:
	/// @param[in] non_negative Clamps the value to zero or more, for properties that do not accept negative values.
	PropertyParserCalc(Units units, bool non_negative);
	virtual ~PropertyParserCalc();

	/// Called to parse a RCSS math function declaration.
	/// @param[out] property The property to set the parsed value on.
	/// @param[in] value The raw value defined for this property.
	/// @param[in] parameters The parameters defined for this property; not used for this parser.
	/// @return True if the value was validated successfully, false otherwise.
	bool ParseValue(Property& property, const String& value, const ParameterMap& parameters) const override;

	/// Parses a math function using the given allowed length, percentage or angle units, or a unitless result when the units are only NUMBER.
	static bool ParseMathFunction(Property& property, const String& value, Units units, bool non_negative);

private:
	Units units;
	bool non_negative;
};

} // namespace Rml
