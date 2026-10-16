#include "ElementBackgroundBorder.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/Box.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Context.h"
#include "../../Include/RmlUi/Core/DecorationTypes.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/MeshUtilities.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/RenderManager.h"
#include "../../Include/RmlUi/Core/StyleSheetTypes.h"
#include "BoxShadowCache.h"
#include "ElementEffects.h"
#include "ElementStyle.h"
#include "GeometryBoxShadow.h"

namespace Rml {

ElementBackgroundBorder::ElementBackgroundBorder() {}

void ElementBackgroundBorder::RenderBackground(Element* element)
{
	if (background_dirty || border_dirty)
	{
		for (auto& background : backgrounds)
		{
			if (background.first != BackgroundType::Background && background.first != BackgroundType::Border)
				background.second.geometry.Release();
		}

		GenerateGeometry(element);

		background_dirty = false;
		border_dirty = false;
	}

	if (Background* background = GetBackground(BackgroundType::Background))
		background->geometry.Render(element->GetAbsoluteOffset(BoxArea::Border));
}

void ElementBackgroundBorder::RenderBorder(Element* element)
{
	const Vector2f offset = element->GetAbsoluteOffset(BoxArea::Border);

	if (Background* shadow = GetBackground(BackgroundType::BoxShadow))
		shadow->box_shadow->geometry.Render(offset, shadow->box_shadow->texture);

	if (Background* border = GetBackground(BackgroundType::Border))
		border->geometry.Render(offset);
}

void ElementBackgroundBorder::DirtyBackground()
{
	background_dirty = true;
}

void ElementBackgroundBorder::DirtyBorder()
{
	border_dirty = true;
}

Geometry* ElementBackgroundBorder::GetClipGeometry(Element* element, BoxArea clip_area)
{
	BackgroundType type = {};
	switch (clip_area)
	{
	case Rml::BoxArea::Border: type = BackgroundType::ClipBorder; break;
	case Rml::BoxArea::Padding: type = BackgroundType::ClipPadding; break;
	case Rml::BoxArea::Content: type = BackgroundType::ClipContent; break;
	default: RMLUI_ERROR; return nullptr;
	}

	RenderManager* render_manager = element->GetRenderManager();
	Geometry& geometry = GetOrCreateBackground(type).geometry;
	if (render_manager && !geometry)
	{
		Mesh mesh = geometry.Release(Geometry::ReleaseMode::ClearMesh);
		MeshUtilities::GenerateBackground(mesh, element->GetRenderBox(clip_area), ColourbPremultiplied(255));
		geometry = render_manager->MakeGeometry(std::move(mesh));
	}

	return &geometry;
}

ElementBackgroundBorder::Background* ElementBackgroundBorder::GetBackground(BackgroundType type)
{
	auto it = backgrounds.find(type);
	if (it != backgrounds.end())
		return &it->second;
	return nullptr;
}

ElementBackgroundBorder::Background& ElementBackgroundBorder::GetOrCreateBackground(BackgroundType type)
{
	auto it = backgrounds.find(type);
	if (it != backgrounds.end())
		return it->second;

	Background& background = backgrounds[type];
	return background;
}

void ElementBackgroundBorder::EraseBackground(BackgroundType type)
{
	backgrounds.erase(type);
}

void ElementBackgroundBorder::GenerateGeometry(Element* element)
{
	RMLUI_ZoneScoped;
	RMLUI_ARC_PERF_ELEMENT_SCOPE(BackgroundBorder, element, BackgroundBorder);
	RMLUI_ARC_PERF_COUNT(BackgroundBorderRegens, 1);
	RenderManager* render_manager = element->GetRenderManager();
	if (!render_manager)
		return;

	const ComputedValues& computed = element->GetComputedValues();

	if (computed.has_box_shadow())
		GetOrCreateBackground(BackgroundType::BoxShadow).box_shadow = BoxShadowCache::GetHandle(element, computed);
	else
		EraseBackground(BackgroundType::BoxShadow);

	const float opacity = computed.opacity();
	ColourbPremultiplied background_color = computed.background_color().ToPremultiplied(opacity);
	Array<ColourbPremultiplied, 4> border_colors = {
		computed.border_top_color().ToPremultiplied(opacity),
		computed.border_right_color().ToPremultiplied(opacity),
		computed.border_bottom_color().ToPremultiplied(opacity),
		computed.border_left_color().ToPremultiplied(opacity),
	};

	// CSS Backgrounds 3: the background colour is clipped to the background-clip box of the bottom-most layer.
	size_t num_layers = 1;
	const Property* decorator = element->GetStyle()->GetLocalPropertyWithResolvedVariables(PropertyId::Decorator);
	if (decorator && decorator->unit == Unit::DECORATOR)
	{
		if (const DecoratorsPtr& decorators = decorator->value.GetReference<DecoratorsPtr>())
			num_layers = Math::Max(decorators->list.size(), size_t(1));
	}
	const BoxArea background_area = ElementEffectsLayers::ToBoxArea(
		ElementEffectsLayers::GetLayerKeyword(ElementEffectsLayers::GetKeywords(element, PropertyId::BackgroundClip), num_layers - 1));

	Geometry& background_geometry = GetOrCreateBackground(BackgroundType::Background).geometry;
	Mesh background_mesh = background_geometry.Release(Geometry::ReleaseMode::ClearMesh);
	for (int i = 0; i < element->GetNumBoxes(); i++)
		MeshUtilities::GenerateBackground(background_mesh, element->GetRenderBox(background_area, i), background_color);
	background_geometry = render_manager->MakeGeometry(std::move(background_mesh));

	Geometry& border_geometry = GetOrCreateBackground(BackgroundType::Border).geometry;
	Mesh border_mesh = border_geometry.Release(Geometry::ReleaseMode::ClearMesh);
	for (int i = 0; i < element->GetNumBoxes(); i++)
		MeshUtilities::GenerateBorder(border_mesh, element->GetRenderBox(BoxArea::Padding, i), border_colors.data());
	border_geometry = render_manager->MakeGeometry(std::move(border_mesh));
}

} // namespace Rml
