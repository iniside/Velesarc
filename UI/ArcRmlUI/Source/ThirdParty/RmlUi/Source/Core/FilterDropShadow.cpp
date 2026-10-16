#include "FilterDropShadow.h"
#include "../../Include/RmlUi/Core/CompiledFilterShader.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/PropertyDictionary.h"
#include "../../Include/RmlUi/Core/RenderManager.h"

namespace Rml {

bool FilterDropShadow::Initialise(const BoxShadow& in_shadow)
{
	shadow = in_shadow;
	if (shadow.blur_radius.unit == Unit::UNKNOWN)
		shadow.blur_radius = NumericValue(0.f, Unit::PX);
	return Any(shadow.offset_x.unit & Unit::LENGTH) && Any(shadow.offset_y.unit & Unit::LENGTH) && Any(shadow.blur_radius.unit & Unit::LENGTH);
}

CompiledFilter FilterDropShadow::CompileFilter(Element* element) const
{
	// The third length is the standard deviation, as in Chromium's filter_effect_builder.cc:298.
	const float sigma = element->ResolveLength(shadow.blur_radius);
	const Vector2f offset = {
		element->ResolveLength(shadow.offset_x),
		element->ResolveLength(shadow.offset_y),
	};

	const ColourbPremultiplied premultiplied = (shadow.current_color ? element->GetComputedValues().color().ToPremultiplied() : shadow.color);
	const Vector4f color = Vector4f(premultiplied.red, premultiplied.green, premultiplied.blue, premultiplied.alpha) / 255.f;

	CompiledFilter filter = element->GetRenderManager()->CompileFilter("drop-shadow",
		Dictionary{{"color", Variant(color)}, {"offset", Variant(offset)}, {"sigma", Variant(sigma)}});

	return filter;
}

void FilterDropShadow::ExtendInkOverflow(Element* element, Rectanglef& scissor_region) const
{
	// Expand the ink overflow area to cover both the native element *and* its offset shadow w/blur.
	const float sigma = element->ResolveLength(shadow.blur_radius);
	const Vector2f offset = {
		element->ResolveLength(shadow.offset_x),
		element->ResolveLength(shadow.offset_y),
	};

	const float blur_extent = 3.f * sigma;
	scissor_region =
		scissor_region.Extend(Math::Max(-offset, Vector2f(0.f)) + Vector2f(blur_extent), Math::Max(offset, Vector2f(0.f)) + Vector2f(blur_extent));
}

FilterDropShadowInstancer::FilterDropShadowInstancer()
{
	// The empty default does not parse, so 'drop-shadow()' without a shadow is a parse error.
	ids.shadow = RegisterProperty("shadow", "").AddParser("text_shadow", "single").GetId();
	RegisterShorthand("filter", "shadow", ShorthandType::RecursiveRepeat);
}

SharedPtr<Filter> FilterDropShadowInstancer::InstanceFilter(const String& /*name*/, const PropertyDictionary& properties)
{
	const Property* p_shadow = properties.GetProperty(ids.shadow);
	if (!p_shadow || p_shadow->unit != Unit::BOXSHADOWLIST)
		return nullptr;

	const BoxShadowList& shadow_list = p_shadow->value.GetReference<BoxShadowList>();
	if (shadow_list.size() != 1)
		return nullptr;

	SharedPtr<FilterDropShadow> filter = MakeShared<FilterDropShadow>();
	if (filter->Initialise(shadow_list.front()))
		return filter;

	return nullptr;
}

} // namespace Rml
