#include "../../Include/RmlUi/SVG/ElementSVG.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Geometry.h"
#include "../../Include/RmlUi/Core/MeshUtilities.h"
#include "../../Include/RmlUi/Core/PropertyIdSet.h"
#include "../../Include/RmlUi/Core/RenderManager.h"
#include "SVGCache.h"

namespace Rml {

RMLUI_RTTI_Define(ElementSVG)

unsigned long ElementSVG::internal_id_counter = 0;

static const String& GetAttributeString(const Element& element, const String& name, String& storage)
{
	if (const Variant* value = element.GetAttribute(name))
	{
		if (value->GetType() == Variant::STRING)
			return value->GetReference<String>();
		storage = value->Get<String>();
		return storage;
	}
	storage.clear();
	return storage;
}

static SharedPtr<SVG::SVGData> RequestHandle(Element* element, const String& source, const String& source_id, const String& svg_element_source,
	bool crop_to_content, Vector2f render_size, const String& stroke_width, bool require_intrinsic)
{
	if (source.empty())
		return SVG::SVGCache::GetHandle(source_id, svg_element_source, SVG::SVGCache::Data, element, crop_to_content, render_size, stroke_width,
			require_intrinsic);
	return SVG::SVGCache::GetHandle(source, source, SVG::SVGCache::File, element, crop_to_content, render_size, stroke_width, require_intrinsic);
}

ElementSVG::ElementSVG(const String& tag) : Element(tag) {}
ElementSVG::~ElementSVG()
{
	handle.reset();
}

bool ElementSVG::GetIntrinsicDimensions(Vector2f& dimensions, float& ratio)
{
	dimensions = Vector2f(0);
	if (!HasAttribute("width") || !HasAttribute("height"))
	{
		String source_storage;
		const bool handle_has_intrinsic = handle && handle->GetIntrinsicDimensions(dimensions);
		if (handle && !handle_has_intrinsic)
			svg_dirty = true;
		if (!handle_has_intrinsic || source_dirty || GetAttributeString(*this, "src", source_storage).empty())
			EnsureSourceLoaded();
		dimensions = Vector2f(0);
		if (handle)
			handle->GetIntrinsicDimensions(dimensions);
	}

	if (HasAttribute("width"))
	{
		dimensions.x = GetAttribute<float>("width", 0);
	}
	if (HasAttribute("height"))
	{
		dimensions.y = GetAttribute<float>("height", 0);
	}

	if (dimensions.y > 0)
		ratio = dimensions.x / dimensions.y;

	dimensions *= ElementUtilities::GetDensityIndependentPixelRatio(this);

	return true;
}

void ElementSVG::OnRender()
{
	Vector2f intrinsic_dimensions;
	if (awaiting_intrinsic && handle && handle->GetIntrinsicDimensions(intrinsic_dimensions))
		svg_dirty = true;
	EnsureSourceLoaded();
	if (handle && handle != ready_handle && handle->IsReady())
	{
		ready_handle = handle;
		ready_geometry = std::move(geometry);
	}
	if (ready_handle)
		ready_geometry.Render(GetAbsoluteOffset(BoxArea::Content), ready_handle->texture);
}

void ElementSVG::OnResize()
{
	if (GetBox().GetSize(BoxArea::Content) != loaded_box_size || ElementUtilities::GetDensityIndependentPixelRatio(this) != loaded_dp_ratio)
		svg_dirty = true;
}

void ElementSVG::OnAttributeChange(const ElementAttributes& changed_attributes)
{
	Element::OnAttributeChange(changed_attributes);

	if (changed_attributes.count("src") || changed_attributes.count("crop-to-content") || changed_attributes.count("stroke-width"))
	{
		svg_dirty = true;
		source_dirty = true;
		DirtyLayout();
	}

	if (changed_attributes.find("width") != changed_attributes.end() || changed_attributes.find("height") != changed_attributes.end())
	{
		DirtyLayout();
	}

	String source_storage;
	if (GetAttributeString(*this, "src", source_storage).empty())
		source_dirty = true;
}

void ElementSVG::OnPropertyChange(const PropertyIdSet& changed_properties)
{
	Element::OnPropertyChange(changed_properties);

	if (changed_properties.Contains(PropertyId::ImageColor) || changed_properties.Contains(PropertyId::Color) ||
		changed_properties.Contains(PropertyId::Opacity) ||
		changed_properties.Contains(PropertyId::ObjectFit) || changed_properties.Contains(PropertyId::ObjectPositionX) ||
		changed_properties.Contains(PropertyId::ObjectPositionY))
	{
		svg_dirty = true;
	}
}

void ElementSVG::GetInnerRML(String& content) const
{
	// If the SVG is from a file source don't add anything to the content string.
	String source_storage;
	if (!GetAttributeString(*this, "src", source_storage).empty())
		return;

	content += svg_data;
}

void ElementSVG::SetInnerRML(const String& content)
{
	// If the SVG is from a file source don't set the svg xml data on the element.
	String source_storage;
	if (!GetAttributeString(*this, "src", source_storage).empty())
		return;

	// We use CreateString instead of std::to_string to avoid having to create an extra std::string and convert it to Rml::String in case clients use
	// a non-std string.
	if (!HasAttribute("rmlui-svgdata-id"))
		SetAttribute("rmlui-svgdata-id", "svgdata:" + CreateString("%lu", internal_id_counter++));

	svg_data = "" + content;
	svg_dirty = true;
	source_dirty = true;
	EnsureSourceLoaded();
}

void ElementSVG::EnsureSourceLoaded()
{
	if (!svg_dirty)
		return;

	svg_dirty = false;

	const bool crop_to_content = HasAttribute("crop-to-content");
	const bool require_intrinsic = !HasAttribute("width") || !HasAttribute("height");
	String source_storage;
	String stroke_width_storage;
	const String& source = GetAttributeString(*this, "src", source_storage);
	const String& stroke_width = GetAttributeString(*this, "stroke-width", stroke_width_storage);
	Vector2f known_intrinsic_dimensions;
	const bool intrinsic_known = handle && !source_dirty && handle->GetIntrinsicDimensions(known_intrinsic_dimensions);
	const bool source_changed = source_dirty;
	source_dirty = false;

	String source_id;
	String svg_element_source;
	if (source.empty())
	{
		const Variant* svgdata_id = GetAttribute("rmlui-svgdata-id");
		source_id = svgdata_id ? svgdata_id->Get<String>() : String("svgdata:undefined");
		if (source_changed)
			handle.reset(); // The old handle won't be re-used so clear it.

		// Build an svg wrapper tag, copying all but src attribute (expected attributes could be width, height, viewBox, etc.)
		svg_element_source = "<svg ";
		for (const ElementAttributes::value_type& attr : GetAttributes())
		{
			if (attr.first == "src")
				continue;
			svg_element_source.append(attr.first);
			svg_element_source.append("=\"");
			svg_element_source.append(StringUtilities::EncodeRml(attr.second.Get<String>()));
			svg_element_source.append("\" ");
		}
		svg_element_source.append(">");
		svg_element_source.append(svg_data);
		svg_element_source.append("</svg>");
	}

	const String& current_source = source.empty() ? svg_element_source : source;
	const bool source_value_changed = current_source != loaded_source;
	loaded_source = current_source;

	const Vector2f box_size = GetBox().GetSize(BoxArea::Content);
	const float dp_ratio = ElementUtilities::GetDensityIndependentPixelRatio(this);
	loaded_box_size = box_size;
	loaded_dp_ratio = dp_ratio;

	SharedPtr<SVG::SVGData> new_handle;
	Rectanglef content_rect = Rectanglef::FromPositionSize(Vector2f(0.f), box_size);
	awaiting_intrinsic = false;
	if (intrinsic_known)
	{
		content_rect = ElementUtilities::GetObjectFitRectangle(this, known_intrinsic_dimensions * dp_ratio);
		new_handle = RequestHandle(this, source, source_id, svg_element_source, crop_to_content, content_rect.Size(), stroke_width, require_intrinsic);
	}
	else
	{
		const SharedPtr<SVG::SVGData> probe_handle =
			RequestHandle(this, source, source_id, svg_element_source, crop_to_content, Vector2f(0.f), stroke_width, require_intrinsic);
		Vector2f intrinsic_dimensions;
		if (probe_handle && probe_handle->GetIntrinsicDimensions(intrinsic_dimensions))
		{
			content_rect = ElementUtilities::GetObjectFitRectangle(this, intrinsic_dimensions * dp_ratio);
			new_handle = RequestHandle(this, source, source_id, svg_element_source, crop_to_content, content_rect.Size(), stroke_width,
				require_intrinsic);
		}
		else if (probe_handle)
		{
			new_handle = RequestHandle(this, source, source_id, svg_element_source, crop_to_content, box_size, stroke_width, require_intrinsic);
			awaiting_intrinsic = static_cast<bool>(new_handle);
		}
	}

	Mesh mesh;
	const Rectanglef box_rect = Rectanglef::FromPositionSize(Vector2f(0.f), box_size);
	if (new_handle && content_rect.Intersects(box_rect))
	{
		const ComputedValues& computed = GetComputedValues();
		const ColourbPremultiplied colour = computed.image_color().ToPremultiplied(computed.opacity());
		const Rectanglef visible_rect = content_rect.Intersect(box_rect);
		const Vector2f texcoords[2] = {
			(visible_rect.TopLeft() - content_rect.TopLeft()) / content_rect.Size(),
			(visible_rect.BottomRight() - content_rect.TopLeft()) / content_rect.Size(),
		};
		MeshUtilities::GenerateQuad(mesh, visible_rect.TopLeft(), visible_rect.Size(), colour, texcoords[0], texcoords[1]);
	}

	handle = std::move(new_handle);
	geometry = Geometry();
	if (RenderManager* render_manager = GetRenderManager())
		geometry = render_manager->MakeGeometry(std::move(mesh));

	if (!handle || handle->HasZeroSize() || source_value_changed)
	{
		ready_handle.reset();
		ready_geometry = Geometry();
	}
	if (handle && handle->IsReady())
	{
		ready_handle = handle;
		ready_geometry = std::move(geometry);
	}
}
} // namespace Rml
