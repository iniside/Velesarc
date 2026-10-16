#include "CalcExpression.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/TypeConverter.h"
#include <algorithm>
#include <cmath>

namespace Rml {

template <typename LeafFunc>
static float EvaluateCalcNodes(const Vector<CalcNode>& nodes, LeafFunc&& leaf_value)
{
	float stack[CalcExpression::MaxNodes];
	size_t size = 0;

	for (const CalcNode& node : nodes)
	{
		switch (node.type)
		{
		case CalcNode::Type::Value: stack[size++] = leaf_value(node.value); break;
		case CalcNode::Type::Add:
			size -= 1;
			stack[size - 1] += stack[size];
			break;
		case CalcNode::Type::Subtract:
			size -= 1;
			stack[size - 1] -= stack[size];
			break;
		case CalcNode::Type::Multiply:
			size -= 1;
			stack[size - 1] *= stack[size];
			break;
		case CalcNode::Type::Divide:
			size -= 1;
			stack[size - 1] /= stack[size];
			break;
		case CalcNode::Type::Min:
		case CalcNode::Type::Max:
		{
			const size_t first = size - node.argument_count;
			float result = stack[first];
			for (size_t i = first + 1; i < size; i++)
				result = (node.type == CalcNode::Type::Min ? Math::Min(result, stack[i]) : Math::Max(result, stack[i]));
			size = first;
			stack[size++] = result;
		}
		break;
		case CalcNode::Type::Clamp:
		{
			size -= 2;
			stack[size - 1] = Math::Max(stack[size - 1], Math::Min(stack[size], stack[size + 1]));
		}
		break;
		}
	}

	RMLUI_ASSERT(size == 1);
	const float result = (size == 1 ? stack[0] : 0.f);
	return std::isfinite(result) ? result : 0.f;
}

CalcExpression::CalcExpression(Vector<CalcNode> in_nodes, bool non_negative) : nodes(std::move(in_nodes)), non_negative(non_negative)
{
	RMLUI_ASSERT(!nodes.empty() && nodes.size() <= MaxNodes);
	for (const CalcNode& node : nodes)
	{
		if (node.type == CalcNode::Type::Value)
			units = units | node.value.unit;
	}
}

float CalcExpression::Resolve(float percent_base) const
{
	const float result = EvaluateCalcNodes(nodes, [percent_base](NumericValue value) {
		if (value.unit == Unit::PERCENT)
			return value.number * 0.01f * percent_base;
		return value.number;
	});
	return non_negative ? Math::Max(result, 0.f) : result;
}

String CalcExpression::ToString() const
{
	enum class Precedence { Atom, Product, Sum };
	struct Entry {
		String text;
		Precedence precedence;
	};
	Vector<Entry> stack;

	auto Operand = [](const Entry& entry, Precedence wrap_from) {
		return entry.precedence >= wrap_from ? "(" + entry.text + ")" : entry.text;
	};

	for (const CalcNode& node : nodes)
	{
		switch (node.type)
		{
		case CalcNode::Type::Value:
			stack.push_back(Entry{Rml::ToString(node.value.number) + Rml::ToString(node.value.unit), Precedence::Atom});
			break;
		case CalcNode::Type::Add:
		case CalcNode::Type::Subtract:
		case CalcNode::Type::Multiply:
		case CalcNode::Type::Divide:
		{
			Entry rhs = std::move(stack.back());
			stack.pop_back();
			Entry& lhs = stack.back();
			switch (node.type)
			{
			case CalcNode::Type::Add: lhs.text = lhs.text + " + " + rhs.text; break;
			case CalcNode::Type::Subtract: lhs.text = lhs.text + " - " + Operand(rhs, Precedence::Sum); break;
			case CalcNode::Type::Multiply: lhs.text = Operand(lhs, Precedence::Sum) + " * " + Operand(rhs, Precedence::Sum); break;
			default: lhs.text = Operand(lhs, Precedence::Sum) + " / " + Operand(rhs, Precedence::Product); break;
			}
			const bool is_sum = (node.type == CalcNode::Type::Add || node.type == CalcNode::Type::Subtract);
			lhs.precedence = (is_sum ? Precedence::Sum : Precedence::Product);
		}
		break;
		case CalcNode::Type::Min:
		case CalcNode::Type::Max:
		case CalcNode::Type::Clamp:
		{
			const size_t count = (node.type == CalcNode::Type::Clamp ? 3 : node.argument_count);
			const size_t first = stack.size() - count;
			String text = (node.type == CalcNode::Type::Min ? "min(" : node.type == CalcNode::Type::Max ? "max(" : "clamp(");
			for (size_t i = first; i < stack.size(); i++)
			{
				if (i != first)
					text += ", ";
				text += stack[i].text;
			}
			text += ')';
			stack.resize(first);
			stack.push_back(Entry{std::move(text), Precedence::Atom});
		}
		break;
		}
	}

	if (stack.size() != 1)
		return String();

	const CalcNode::Type root = nodes.back().type;
	if (root == CalcNode::Type::Min || root == CalcNode::Type::Max || root == CalcNode::Type::Clamp)
		return stack[0].text;
	return "calc(" + stack[0].text + ")";
}

CalcExpressionPtr ResolveCalcLengths(const CalcExpressionPtr& expression, const Function<float(NumericValue)>& resolve_length)
{
	if (!expression)
		return expression;

	auto NeedsResolve = [](const CalcNode& node) { return node.type == CalcNode::Type::Value && Any(node.value.unit & Unit::LENGTH) && node.value.unit != Unit::DEVICE_PX; };

	const Vector<CalcNode>& nodes = expression->GetNodes();
	if (std::none_of(nodes.begin(), nodes.end(), NeedsResolve))
		return expression;

	Vector<CalcNode> resolved = nodes;
	for (CalcNode& node : resolved)
	{
		if (NeedsResolve(node))
			node.value = NumericValue(resolve_length(node.value), Unit::DEVICE_PX);
	}
	return MakeShared<const CalcExpression>(std::move(resolved), expression->IsNonNegative());
}

bool LinearizeCalcExpression(const CalcExpression& expression, Vector<NumericValue>& out_terms)
{
	using Terms = Vector<NumericValue>;
	Vector<Terms> stack;

	auto IsNumber = [](const Terms& terms) { return terms.size() == 1 && terms[0].unit == Unit::NUMBER; };
	auto Scale = [](Terms& terms, float factor) {
		for (NumericValue& term : terms)
			term.number *= factor;
	};
	auto Accumulate = [](Terms& terms, const Terms& other, float sign) {
		for (const NumericValue& other_term : other)
		{
			auto it = std::find_if(terms.begin(), terms.end(), [&](const NumericValue& term) { return term.unit == other_term.unit; });
			if (it != terms.end())
				it->number += sign * other_term.number;
			else
				terms.push_back(NumericValue(sign * other_term.number, other_term.unit));
		}
	};

	for (const CalcNode& node : expression.GetNodes())
	{
		switch (node.type)
		{
		case CalcNode::Type::Value: stack.push_back(Terms{node.value}); break;
		case CalcNode::Type::Add:
		case CalcNode::Type::Subtract:
		case CalcNode::Type::Multiply:
		case CalcNode::Type::Divide:
		{
			if (stack.size() < 2)
				return false;
			Terms rhs = std::move(stack.back());
			stack.pop_back();
			Terms& lhs = stack.back();
			if (node.type == CalcNode::Type::Add || node.type == CalcNode::Type::Subtract)
			{
				Accumulate(lhs, rhs, node.type == CalcNode::Type::Add ? 1.f : -1.f);
			}
			else if (node.type == CalcNode::Type::Multiply)
			{
				if (IsNumber(rhs))
				{
					Scale(lhs, rhs[0].number);
				}
				else if (IsNumber(lhs))
				{
					const float factor = lhs[0].number;
					lhs = std::move(rhs);
					Scale(lhs, factor);
				}
				else
					return false;
			}
			else
			{
				if (!IsNumber(rhs) || rhs[0].number == 0.f)
					return false;
				Scale(lhs, 1.f / rhs[0].number);
			}
		}
		break;
		case CalcNode::Type::Min:
		case CalcNode::Type::Max:
		case CalcNode::Type::Clamp:
		{
			const size_t count = (node.type == CalcNode::Type::Clamp ? 3 : node.argument_count);
			if (count == 0 || stack.size() < count)
				return false;
			const size_t first = stack.size() - count;
			Vector<CalcNode> argument_nodes;
			for (size_t i = first; i < stack.size(); i++)
			{
				if (stack[i].size() != 1 || stack[i][0].unit != stack[first][0].unit)
					return false;
				argument_nodes.push_back(CalcNode{CalcNode::Type::Value, 0, stack[i][0]});
			}
			argument_nodes.push_back(node);
			const float result = EvaluateCalcNodes(argument_nodes, [](NumericValue value) { return value.number; });
			const Unit unit = stack[first][0].unit;
			stack.resize(first);
			stack.push_back(Terms{NumericValue(result, unit)});
		}
		break;
		}
	}

	if (stack.size() != 1)
		return false;

	out_terms = std::move(stack[0]);
	return true;
}

bool FoldCalcExpression(const CalcExpression& expression, NumericValue& out_value)
{
	const uint64_t dimension_units = static_cast<uint64_t>(expression.GetUnits()) & ~static_cast<uint64_t>(Unit::NUMBER);
	if ((dimension_units & (dimension_units - 1)) != 0)
		return false;

	out_value.number = EvaluateCalcNodes(expression.GetNodes(), [](NumericValue value) { return value.number; });
	out_value.unit = (dimension_units == 0 ? Unit::NUMBER : static_cast<Unit>(dimension_units));
	return true;
}

CalcExpressionPtr MixCalcExpressions(const CalcExpression& a, const CalcExpression& b, float alpha)
{
	const Vector<CalcNode>& nodes_a = a.GetNodes();
	const Vector<CalcNode>& nodes_b = b.GetNodes();
	if (nodes_a.size() + nodes_b.size() + 5 > CalcExpression::MaxNodes)
		return nullptr;

	Vector<CalcNode> nodes;
	nodes.reserve(nodes_a.size() + nodes_b.size() + 5);
	nodes.insert(nodes.end(), nodes_a.begin(), nodes_a.end());
	nodes.push_back(CalcNode{CalcNode::Type::Value, 0, NumericValue(1.f - alpha, Unit::NUMBER)});
	nodes.push_back(CalcNode{CalcNode::Type::Multiply, 0, NumericValue()});
	nodes.insert(nodes.end(), nodes_b.begin(), nodes_b.end());
	nodes.push_back(CalcNode{CalcNode::Type::Value, 0, NumericValue(alpha, Unit::NUMBER)});
	nodes.push_back(CalcNode{CalcNode::Type::Multiply, 0, NumericValue()});
	nodes.push_back(CalcNode{CalcNode::Type::Add, 0, NumericValue()});
	return MakeShared<const CalcExpression>(std::move(nodes), a.IsNonNegative() || b.IsNonNegative());
}

CalcExpressionPtr MakeCalcExpression(NumericValue value)
{
	return MakeShared<const CalcExpression>(Vector<CalcNode>{CalcNode{CalcNode::Type::Value, 0, value}});
}

} // namespace Rml
