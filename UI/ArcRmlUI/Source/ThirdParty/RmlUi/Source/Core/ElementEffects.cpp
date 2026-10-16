#include "ElementEffects.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Decorator.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementDocument.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Filter.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/RenderManager.h"
#include "../../Include/RmlUi/Core/StyleSheet.h"
#include "../../Include/RmlUi/Core/StyleSheetSpecification.h"
#include "ElementBackgroundBorder.h"
#include "ElementStyle.h"
#include "TransformState.h"

namespace Rml {

namespace ElementEffectsLayers {

	StringList GetKeywords(Element* element, PropertyId id)
	{
		const Property* property = element->GetStyle()->GetLocalPropertyWithResolvedVariables(id);
		if (!property || property->unit != Unit::STRING)
			property = StyleSheetSpecification::GetProperty(id)->GetDefaultValue();

		StringList keywords;
		StringUtilities::ExpandString(keywords, property->Get<String>(), ',');
		return keywords;
	}

	const String& GetLayerKeyword(const StringList& keywords, size_t layer_index)
	{
		static const String empty;
		return keywords.empty() ? empty : keywords[layer_index % keywords.size()];
	}

	BoxArea ToBoxArea(const String& box)
	{
		if (box == "padding-box")
			return BoxArea::Padding;
		if (box == "content-box")
			return BoxArea::Content;
		return BoxArea::Border;
	}

} // namespace ElementEffectsLayers

namespace ElementEffectsMaskComposite {

	BlendMode ToBlendMode(const String& composite)
	{
		if (composite == "subtract")
			return BlendMode::SourceOut;
		if (composite == "intersect")
			return BlendMode::SourceIn;
		if (composite == "exclude")
			return BlendMode::Xor;
		return BlendMode::Blend;
	}

} // namespace ElementEffectsMaskComposite

ElementEffects::ElementEffects(Element* _element) : element(_element) {}

ElementEffects::~ElementEffects()
{
	ReleaseEffects();
}

void ElementEffects::InstanceEffects()
{
	if (!effects_dirty)
		return;

	effects_dirty = false;
	effects_data_dirty = true;

	RMLUI_ZoneScopedC(0xB22222);
	RMLUI_ARC_PERF_ELEMENT_SCOPE(EffectsInstance, element, Effects);
	RMLUI_ARC_PERF_COUNT(EffectsInstanced, 1);
	ReleaseEffects();

	RenderManager* render_manager = element->GetRenderManager();
	if (!render_manager)
	{
		RMLUI_ERRORMSG("Decorators are being instanced before a render manager is available. Is this element attached to the document?");
		return;
	}

	const ComputedValues& computed = element->GetComputedValues();

	if (computed.has_decorator() || computed.has_mask_image())
	{
		const StyleSheet* style_sheet = element->GetStyleSheet();
		if (!style_sheet)
			return;

		for (const auto id : {PropertyId::Decorator, PropertyId::MaskImage})
		{
			const Property* property = element->GetStyle()->GetLocalPropertyWithResolvedVariables(id);
			if (!property || property->unit != Unit::DECORATOR)
				continue;

			DecoratorsPtr decorators_ptr = property->Get<DecoratorsPtr>();
			if (!decorators_ptr)
				continue;

			PropertySource document_source("", 0, "");
			const PropertySource* source = property->source.get();

			if (!source)
			{
				if (ElementDocument* document = element->GetOwnerDocument())
				{
					document_source.path = document->GetSourceURL();
					source = &document_source;
				}
			}

			const DecoratorPtrList& decorator_list = style_sheet->InstanceDecorators(*render_manager, *decorators_ptr, source);
			RMLUI_ASSERT(decorator_list.empty() || decorator_list.size() == decorators_ptr->list.size());

			DecoratorEntryList& decorators_target = (id == PropertyId::Decorator ? decorators : mask_images);
			decorators_target.reserve(decorators_ptr->list.size());

			const bool is_mask = (id == PropertyId::MaskImage);
			StringList composites;
			if (is_mask)
				composites = ElementEffectsLayers::GetKeywords(element, PropertyId::MaskComposite);
			const StringList origins = ElementEffectsLayers::GetKeywords(element, is_mask ? PropertyId::MaskOrigin : PropertyId::BackgroundOrigin);
			const StringList clips = ElementEffectsLayers::GetKeywords(element, is_mask ? PropertyId::MaskClip : PropertyId::BackgroundClip);

			for (size_t i = 0; i < decorator_list.size() && i < decorators_ptr->list.size(); i++)
			{
				const SharedPtr<const Decorator>& decorator = decorator_list[i];
				if (decorator)
				{
					DecoratorEntry entry;
					entry.decorator_data = 0;
					entry.decorator = decorator;
					entry.layer = decorators_ptr->list[i].layer;
					entry.layer.origin = ElementEffectsLayers::ToBoxArea(ElementEffectsLayers::GetLayerKeyword(origins, i));
					entry.paint_area = ElementEffectsLayers::ToBoxArea(ElementEffectsLayers::GetLayerKeyword(clips, i));
					if (is_mask)
						entry.composite = ElementEffectsMaskComposite::ToBlendMode(ElementEffectsLayers::GetLayerKeyword(composites, i));

					RMLUI_ASSERT(entry.paint_area >= BoxArea::Border && entry.paint_area <= BoxArea::Content);
					decorators_target.push_back(std::move(entry));
				}
			}
		}
	}

	if (computed.has_filter() || computed.has_backdrop_filter())
	{
		for (const auto id : {PropertyId::Filter, PropertyId::BackdropFilter})
		{
			const Property* property = element->GetStyle()->GetLocalPropertyWithResolvedVariables(id);
			if (!property || property->unit != Unit::FILTER)
				continue;

			FiltersPtr filters_ptr = property->Get<FiltersPtr>();
			if (!filters_ptr)
				continue;

			FilterEntryList& list = (id == PropertyId::Filter ? filters : backdrop_filters);
			list.reserve(filters_ptr->list.size());

			for (const FilterDeclaration& declaration : filters_ptr->list)
			{
				SharedPtr<const Filter> filter = declaration.instancer->InstanceFilter(declaration.type, declaration.properties);
				if (filter)
				{
					list.push_back({std::move(filter), CompiledFilter{}});
				}
				else
				{
					const auto& source = property->source;
					Log::Message(Log::LT_WARNING, "Filter '%s' in '%s' could not be instanced, declared at %s:%d", declaration.type.c_str(),
						filters_ptr->value.c_str(), source ? source->path.c_str() : "", source ? source->line_number : -1);
				}
			}
		}
	}
}

void ElementEffects::ReloadEffectsData()
{
	if (decorator_textures_pending && !effects_data_dirty)
	{
		for (const DecoratorEntryList* list : {&decorators, &mask_images})
		{
			for (const DecoratorEntry& decorator : *list)
			{
				if (decorator.textures_pending && decorator.decorator->AreTexturesReady())
					effects_data_dirty = true;
			}
		}
	}

	if (effects_data_dirty)
	{
		RMLUI_ARC_PERF_ELEMENT_SCOPE(EffectsData, element, Effects);
		RMLUI_ARC_PERF_COUNT(EffectsDataReloads, 1);
		effects_data_dirty = false;
		decorator_textures_pending = false;

		bool decorator_data_failed = false;
		for (DecoratorEntryList* list : {&decorators, &mask_images})
		{
			for (DecoratorEntry& decorator : *list)
			{
				const DecoratorDataHandle old_data = decorator.decorator_data;

				decorator.textures_pending = !decorator.decorator->AreTexturesReady();
				decorator_textures_pending |= decorator.textures_pending;
				if (decorator.textures_pending)
				{
					decorator.decorator_data = 0;
				}
				else
				{
					decorator.decorator_data = decorator.decorator->GenerateLayerElementData(element, decorator.paint_area, decorator.layer);
					if (!decorator.decorator_data)
						decorator_data_failed = true;
				}

				// Release old element data after generating new data, so that the decorator can reuse any cache.
				if (old_data)
					decorator.decorator->ReleaseElementData(old_data);
			}
		}

		if (decorator_data_failed)
			Log::Message(Log::LT_WARNING, "Could not generate decorator element data: %s", element->GetAddress().c_str());

		bool filter_compile_failed = false;
		for (FilterEntryList* list : {&filters, &backdrop_filters})
		{
			for (FilterEntry& filter : *list)
			{
				filter.compiled = filter.filter->CompileFilter(element);
				if (!filter.compiled)
					filter_compile_failed = true;
			}
		}

		if (filter_compile_failed)
			Log::Message(Log::LT_WARNING, "Could not compile filter on element: %s", element->GetAddress().c_str());
	}
}

void ElementEffects::ReleaseEffects()
{
	for (DecoratorEntryList* list : {&decorators, &mask_images})
	{
		for (DecoratorEntry& decorator : *list)
		{
			if (decorator.decorator_data)
				decorator.decorator->ReleaseElementData(decorator.decorator_data);
		}
		list->clear();
	}
	decorator_textures_pending = false;

	filters.clear();
	backdrop_filters.clear();
}

void ElementEffects::RenderEffects(RenderStage render_stage)
{
	InstanceEffects();
	ReloadEffectsData();

	if (!decorators.empty())
	{
		if (render_stage == RenderStage::Decoration)
		{
			// Render the decorators attached to this element in its current state.
			// Render from back to front for correct render order.
			for (int i = (int)decorators.size() - 1; i >= 0; i--)
			{
				DecoratorEntry& decorator = decorators[i];
				if (decorator.decorator_data)
					decorator.decorator->RenderElement(element, decorator.decorator_data);
			}
		}
	}

	if (filters.empty() && backdrop_filters.empty() && mask_images.empty())
		return;

	RenderManager* render_manager = element->GetRenderManager();
	if (!render_manager)
		return;

	Rectanglei initial_scissor_region = render_manager->GetScissorRegion();

	auto ApplyClippingRegion = [this, &render_manager](PropertyId filter_id) {
		RMLUI_ASSERT(filter_id == PropertyId::Filter || filter_id == PropertyId::BackdropFilter);

		const bool force_clip_to_self_border_box = (filter_id == PropertyId::BackdropFilter);
		ElementUtilities::SetClippingRegion(element, force_clip_to_self_border_box);

		// Find the region being affected by the active filters and apply it as a scissor.
		Rectanglef filter_region = Rectanglef::MakeInvalid();
		ElementUtilities::GetBoundingBox(filter_region, element, force_clip_to_self_border_box ? BoxArea::Border : BoxArea::Auto);

		// The filter property may draw outside our normal clipping region due to ink overflow.
		if (filter_id == PropertyId::Filter)
		{
			for (const auto& filter : filters)
				filter.filter->ExtendInkOverflow(element, filter_region);
		}

		Math::ExpandToPixelGrid(filter_region);

		Rectanglei scissor_region = Rectanglei(filter_region).IntersectIfValid(render_manager->GetScissorRegion());
		render_manager->SetScissorRegion(scissor_region);
	};
	auto ApplyScissorRegionForBackdrop = [this, &render_manager]() {
		// Set the scissor region for backdrop drawing, which covers the element's border box plus any area we may need
		// to read from, such as any blur radius.
		Rectanglef filter_region = Rectanglef::MakeInvalid();
		ElementUtilities::GetBoundingBox(filter_region, element, BoxArea::Border);
		for (const auto& filter : backdrop_filters)
			filter.filter->ExtendInkOverflow(element, filter_region);
		Math::ExpandToPixelGrid(filter_region);
		render_manager->SetScissorRegion(Rectanglei(filter_region));
	};

	if (render_stage == RenderStage::Enter)
	{
		const LayerHandle backdrop_source_layer = render_manager->GetTopLayer();

		if (!filters.empty() || !mask_images.empty())
		{
			render_manager->PushLayer();
		}

		if (!backdrop_filters.empty())
		{
			const LayerHandle backdrop_destination_layer = render_manager->GetTopLayer();

			// @performance We strictly only need this temporary buffer when having to read from outside the element
			// boundaries, which currently only applies to blur and drop-shadow. Alternatively, we could avoid this
			// completely if we introduced a render interface API concept of different input and output clipping. That
			// is, we set a large input scissor to cover all input data, which can be used e.g. during blurring, and use
			// our small border-area-only clipping region for the composite layers output.
			ApplyScissorRegionForBackdrop();
			render_manager->PushLayer();
			const LayerHandle backdrop_temp_layer = render_manager->GetTopLayer();

			FilterHandleList filter_handles;
			for (auto& filter : backdrop_filters)
				filter.compiled.AddHandleTo(filter_handles);

			// Render the backdrop filters in the extended scissor region including any ink overflow.
			render_manager->CompositeLayers(backdrop_source_layer, backdrop_temp_layer, BlendMode::Blend, filter_handles);

			// Then composite the filter output to our destination while applying our clipping region, including any border-radius.
			ApplyClippingRegion(PropertyId::BackdropFilter);
			render_manager->CompositeLayers(backdrop_temp_layer, backdrop_destination_layer, BlendMode::Blend, {});
			render_manager->PopLayer();
			render_manager->SetScissorRegion(initial_scissor_region);
		}
	}
	else if (render_stage == RenderStage::Exit)
	{
		if (!filters.empty() || !mask_images.empty())
		{
			ApplyClippingRegion(PropertyId::Filter);

			CompiledFilter mask_image_filter;
			FilterHandleList filter_handles;
			filter_handles.reserve(filters.size() + (mask_images.empty() ? 0 : 1));

			for (auto& filter : filters)
				filter.compiled.AddHandleTo(filter_handles);

			if (!mask_images.empty())
			{
				render_manager->PushLayer();

				bool painted_mask_layer = false;
				for (int i = (int)mask_images.size() - 1; i >= 0; i--)
				{
					DecoratorEntry& mask_image = mask_images[i];
					if (!mask_image.decorator_data)
						continue;

					// CSS Masking 1: the compositing operator of the bottom-most painted layer is ignored.
					const BlendMode composite = (painted_mask_layer ? mask_image.composite : BlendMode::Blend);
					painted_mask_layer = true;

					if (composite == BlendMode::Blend)
					{
						mask_image.decorator->RenderElement(element, mask_image.decorator_data);
						continue;
					}

					// The operator applies only inside the layer's mask-clip box; the accumulated mask outside it is kept.
					Geometry* layer_clip_geometry = element->GetElementBackgroundBorder()->GetClipGeometry(element, mask_image.paint_area);
					if (!layer_clip_geometry || !*layer_clip_geometry)
						continue;

					render_manager->PushLayer();
					mask_image.decorator->RenderElement(element, mask_image.decorator_data);

					const ClipMaskGeometryList element_clip_mask_list = render_manager->GetState().clip_mask_list;
					const TransformState* transform_state = element->GetTransformState();
					ClipMaskGeometryList layer_clip_mask_list = element_clip_mask_list;
					layer_clip_mask_list.push_back(ClipMaskGeometry{
						layer_clip_mask_list.empty() ? ClipMaskOperation::Set : ClipMaskOperation::Intersect,
						layer_clip_geometry,
						element->GetAbsoluteOffset(BoxArea::Border).Round(),
						transform_state ? transform_state->GetTransform() : nullptr,
					});
					render_manager->SetClipMask(std::move(layer_clip_mask_list));
					render_manager->CompositeLayers(render_manager->GetTopLayer(), render_manager->GetNextLayer(), composite, {});
					render_manager->SetClipMask(element_clip_mask_list);
					render_manager->PopLayer();
				}
				mask_image_filter = render_manager->SaveLayerAsMaskImage();
				mask_image_filter.AddHandleTo(filter_handles);
				render_manager->PopLayer();
			}

			render_manager->CompositeLayers(render_manager->GetTopLayer(), render_manager->GetNextLayer(), BlendMode::Blend, filter_handles);
			render_manager->PopLayer();
			render_manager->SetScissorRegion(initial_scissor_region);
		}
	}
}

void ElementEffects::DirtyEffects()
{
	effects_dirty = true;
}

void ElementEffects::DirtyEffectsData()
{
	effects_data_dirty = true;
}

} // namespace Rml
