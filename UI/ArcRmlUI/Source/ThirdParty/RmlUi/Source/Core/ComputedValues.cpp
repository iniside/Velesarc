#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "CalcExpression.h"
#include "ComputeProperty.h"
#include "ElementStyle.h"

namespace Rml {

const AnimationList* Style::ComputedValues::animation() const
{
	if (auto p = GetLocalPropertyWithResolvedVariables(PropertyId::Animation))
	{
		if (p->unit == Unit::ANIMATION)
			return &(p->value.GetReference<AnimationList>());
	}
	return nullptr;
}

const TransitionList* Style::ComputedValues::transition() const
{
	if (auto p = GetLocalPropertyWithResolvedVariables(PropertyId::Transition))
	{
		if (p->unit == Unit::TRANSITION)
			return &(p->value.GetReference<TransitionList>());
	}
	return nullptr;
}

String Style::ComputedValues::font_family() const
{
	if (auto p = element->GetProperty(PropertyId::FontFamily))
		return ComputeFontFamily(p->Get<String>());

	return String();
}

String Style::ComputedValues::cursor() const
{
	if (auto p = element->GetProperty(PropertyId::Cursor))
		return p->Get<String>();

	return String();
}

float Style::ComputedValues::letter_spacing() const
{
	if (inherited.has_letter_spacing)
	{
		if (auto p = element->GetProperty(PropertyId::LetterSpacing))
			return element->ResolveLength(p->GetNumericValue());
	}
	return 0.f;
}

CornerSizes Style::ComputedValues::border_radius(Vector2f border_box_size) const
{
	auto ResolveRadius = [border_box_size](LengthPercentage radius) {
		if (radius.type == LengthPercentage::Length)
			return radius.value;
		return Math::Min(ResolveValue(radius, border_box_size.x), ResolveValue(radius, border_box_size.y));
	};
	return {ResolveRadius(border_top_left_radius()), ResolveRadius(border_top_right_radius()), ResolveRadius(border_bottom_right_radius()),
		ResolveRadius(border_bottom_left_radius())};
}

bool Style::ComputedValues::has_border_radius() const
{
	for (const LengthPercentage radius : {border_top_left_radius(), border_top_right_radius(), border_bottom_right_radius(), border_bottom_left_radius()})
	{
		if (radius.type != LengthPercentage::Length || radius.value > 0.f)
			return true;
	}
	return false;
}

const Property* ComputedValues::GetLocalPropertyWithResolvedVariables(PropertyId id) const
{
	return element->GetStyle()->GetLocalPropertyWithResolvedVariables(id);
}

float ResolveValueOr(Style::LengthPercentageAuto length, float base_value, float default_value)
{
	if (length.type == Style::LengthPercentageAuto::Length)
		return length.value;
	else if (length.type == Style::LengthPercentageAuto::Percentage && base_value >= 0.f)
		return length.value * 0.01f * base_value;
	else if (length.type == Style::LengthPercentageAuto::Calc && length.calc && base_value >= 0.f)
		return length.calc->Resolve(base_value);
	return default_value;
}

float ResolveValueOr(Style::LengthPercentage length, float base_value, float default_value)
{
	if (length.type == Style::LengthPercentage::Length)
		return length.value;
	else if (length.type == Style::LengthPercentage::Percentage && base_value >= 0.f)
		return length.value * 0.01f * base_value;
	else if (length.type == Style::LengthPercentage::Calc && length.calc && base_value >= 0.f)
		return length.calc->Resolve(base_value);
	return default_value;
}

} // namespace Rml
