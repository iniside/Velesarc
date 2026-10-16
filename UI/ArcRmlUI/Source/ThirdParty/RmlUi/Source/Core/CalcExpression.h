#pragma once

#include "../../Include/RmlUi/Core/NumericValue.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

struct CalcNode {
	enum class Type : uint8_t { Value, Add, Subtract, Multiply, Divide, Min, Max, Clamp };
	Type type = Type::Value;
	uint16_t argument_count = 0;
	NumericValue value;

	bool operator==(const CalcNode& other) const { return type == other.type && argument_count == other.argument_count && value == other.value; }
};

/**
    An immutable calc(), min(), max() or clamp() expression over lengths, percentages and numbers, stored in postfix order.
 */
class CalcExpression {
public:
	static constexpr size_t MaxNodes = 64;

	/// @param[in] non_negative Clamps the resolved value to zero or more, for properties that do not accept negative values.
	explicit CalcExpression(Vector<CalcNode> nodes, bool non_negative = false);

	const Vector<CalcNode>& GetNodes() const { return nodes; }
	Units GetUnits() const { return units; }
	bool HasPercentage() const { return Any(units & Unit::PERCENT); }
	bool IsNonNegative() const { return non_negative; }

	/// Evaluates the expression to pixels. All length leaves must already be resolved to 'px'.
	/// @param[in] percent_base The value that percentages are relative to.
	float Resolve(float percent_base) const;

	String ToString() const;

	bool operator==(const CalcExpression& other) const { return nodes == other.nodes && non_negative == other.non_negative; }

private:
	Vector<CalcNode> nodes;
	Units units = Unit::UNKNOWN;
	bool non_negative = false;
};

/// Returns the expression with every length leaf other than 'px' converted to pixels, or the input expression when there are none.
CalcExpressionPtr ResolveCalcLengths(const CalcExpressionPtr& expression, const Function<float(NumericValue)>& resolve_length);

/// Decomposes a linear expression into one value per unit, such as 'calc(-50% + 10px)' into '-50%' and '10px'.
/// @return False if the expression applies min(), max() or clamp() over different units.
bool LinearizeCalcExpression(const CalcExpression& expression, Vector<NumericValue>& out_terms);

/// Evaluates an expression whose length leaves all share one unit, producing a value in that unit.
/// @return False if the expression mixes several length or percentage units.
bool FoldCalcExpression(const CalcExpression& expression, NumericValue& out_value);

/// Creates the expression 'a * (1 - alpha) + b * alpha', or returns null if the result would exceed the node limit.
CalcExpressionPtr MixCalcExpressions(const CalcExpression& a, const CalcExpression& b, float alpha);

CalcExpressionPtr MakeCalcExpression(NumericValue value);

} // namespace Rml
