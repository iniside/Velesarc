#include "../../Include/RmlUi/Core/ElementScroll.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Context.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Event.h"
#include "../../Include/RmlUi/Core/Factory.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "ComputeProperty.h"
#include "ElementStyle.h"
#include "Layout/LayoutDetails.h"
#include "Layout/LayoutNode.h"
#include "WidgetScroll.h"

namespace Rml {

namespace ElementScrollFluent {
	// Chromium's Fluent scrollbar metrics: ui/native_theme/native_theme_fluent.{h,cc} and third_party/blink/renderer/core/scroll/
	// scrollbar_theme_{aura,fluent}.cc (BSD-3-Clause, Source/Core/LICENSE-chromium.txt).
	constexpr float TrackThickness = 15.f;
	constexpr float ButtonLength = 18.f;
	constexpr float ThumbThickness = 9.f;
	constexpr float ThumbMinimumLength = 17.f;
	constexpr float ArrowSide = 9.f;
	constexpr float ThinProportion = 2.f / 3.f;
	constexpr int ScrollbarWidthThin = 1;
	constexpr int ScrollbarWidthNone = 2;

	struct Metrics {
		float thickness = 0.f;
		float thumb_thickness = 0.f;
		float button_length = 0.f;
		float thumb_minimum_length = 0.f;
		float arrow_side = 0.f;
		float arrow_offset = 0.f;
	};

	static Metrics GetMetrics(int scrollbar_width, float dp_ratio)
	{
		Metrics metrics;
		if (scrollbar_width == ScrollbarWidthNone)
			return metrics;

		const float proportion = (scrollbar_width == ScrollbarWidthThin ? ThinProportion : 1.f) * dp_ratio;
		metrics.thickness = Math::Round(TrackThickness * proportion);
		const float thumb_thickness = Math::Round(ThumbThickness * proportion);
		metrics.thumb_thickness = thumb_thickness - float(int(metrics.thickness - thumb_thickness) % 2);
		metrics.button_length = Math::Round(ButtonLength * proportion);
		metrics.thumb_minimum_length = Math::RoundDown(ThumbMinimumLength * proportion);
		const float arrow_scale = Math::Max(metrics.thickness, metrics.button_length) / ButtonLength;
		metrics.arrow_side = Math::RoundUp(ArrowSide * arrow_scale);
		metrics.arrow_offset = Math::Round(arrow_scale);
		return metrics;
	}

	// Track and thumb colours of the light Fluent theme, measured in headless Chrome 154.
	static Colourb GetDefaultTrackColor()
	{
		return Colourb(252, 252, 252);
	}
	static Colourb GetDefaultThumbColor()
	{
		return Colourb(139, 139, 139);
	}

	static Colourb GetColor(Element* element, PropertyId id, Colourb default_color)
	{
		const Property* property = element->GetProperty(id);
		if (property && IsCurrentColorKeyword(id, *property))
			return element->GetComputedValues().color();
		return (property && property->unit == Unit::COLOUR ? property->Get<Colourb>() : default_color);
	}

	static float ToPercent(float length, float base)
	{
		return base != 0.f ? 100.f * length / base : 0.f;
	}

	// Approximates Chrome's Segoe Fluent Icons arrow glyph with a conic-gradient wedge of measured proportions, centred in the arrow rectangle.
	static String GetArrowDecorator(bool vertical, bool increment, const Metrics& metrics, Colourb color)
	{
		constexpr float ArrowDepthRatio = 0.78f;
		constexpr float ArrowAngle = 65.3f;

		const float side = metrics.arrow_side;
		const float depth = side * ArrowDepthRatio;
		const Vector2f button = (vertical ? Vector2f(metrics.thickness, metrics.button_length) : Vector2f(metrics.button_length, metrics.thickness));
		Vector2f position = (button - Vector2f(side)) * 0.5f;
		const Vector2f size = (vertical ? Vector2f(side, depth) : Vector2f(depth, side));
		const float offset = (increment ? metrics.arrow_offset : -metrics.arrow_offset) + 0.5f * (side - depth);

		float angle = 0.f;
		const char* apex = "";
		if (vertical)
		{
			position.y += offset;
			angle = (increment ? -0.5f * ArrowAngle : 180.f - 0.5f * ArrowAngle);
			apex = (increment ? "50% 100%" : "50% 0%");
		}
		else
		{
			position.x += offset;
			angle = (increment ? 270.f - 0.5f * ArrowAngle : 90.f - 0.5f * ArrowAngle);
			apex = (increment ? "100% 50%" : "0% 50%");
		}

		return CreateString("conic-gradient(from %gdeg at %s, rgba(%d, %d, %d, %g) %gdeg, transparent %gdeg) %g%% %g%% / %g%% %g%% no-repeat", angle,
			apex, color.red, color.green, color.blue, color.alpha / 255.f, ArrowAngle, ArrowAngle, ToPercent(position.x, button.x - size.x),
			ToPercent(position.y, button.y - size.y), ToPercent(size.x, button.x), ToPercent(size.y, button.y));
	}
} // namespace ElementScrollFluent

ElementScroll::ElementScroll(Element* _element)
{
	element = _element;
	corner = nullptr;
}

ElementScroll::~ElementScroll() {}

void ElementScroll::Update()
{
	for (int i = 0; i < 2; i++)
	{
		if (scrollbars[i].widget != nullptr)
		{
			RMLUI_ARC_PERF_SCOPE(ElementScroll);
			scrollbars[i].widget->Update();
		}
	}
}

void ElementScroll::EnableScrollbar(Orientation orientation, float element_width)
{
	if (!scrollbars[orientation].enabled)
	{
		CreateScrollbar(orientation);
		scrollbars[orientation].element->SetProperty(PropertyId::Visibility, Property(Style::Visibility::Visible));
		scrollbars[orientation].enabled = true;
	}

	// Determine the size of the scrollbar.
	Box box;
	LayoutDetails::BuildScrollbarBox(box, Vector2f(element_width, element_width), scrollbars[orientation].element);

	if (orientation == VERTICAL)
		scrollbars[orientation].size = box.GetSize(BoxArea::Margin).x;
	if (orientation == HORIZONTAL)
		scrollbars[orientation].size = box.GetSize(BoxArea::Margin).y;
}

void ElementScroll::DisableScrollbar(Orientation orientation)
{
	if (scrollbars[orientation].enabled)
	{
		scrollbars[orientation].element->SetProperty(PropertyId::Visibility, Property(Style::Visibility::Hidden));
		scrollbars[orientation].enabled = false;

		if (corner)
			corner->SetProperty(PropertyId::Visibility, Property(Style::Visibility::Hidden));
	}
}

void ElementScroll::UpdateScrollbar(Orientation orientation)
{
	float bar_position;
	float traversable_track;
	if (orientation == VERTICAL)
	{
		bar_position = element->GetScrollTop();
		traversable_track = element->GetScrollHeight() - element->GetClientHeight();
	}
	else
	{
		bar_position = element->GetScrollLeft();
		traversable_track = element->GetScrollWidth() - element->GetClientWidth();
	}

	if (traversable_track <= 0)
		bar_position = 0;
	else
		bar_position /= traversable_track;

	if (scrollbars[orientation].widget != nullptr)
	{
		bar_position = Math::Clamp(bar_position, 0.0f, 1.0f);

		if (scrollbars[orientation].widget->GetBarPosition() != bar_position)
			scrollbars[orientation].widget->SetBarPosition(bar_position);
	}
}

Element* ElementScroll::GetScrollbar(Orientation orientation)
{
	return scrollbars[orientation].element;
}

float ElementScroll::GetScrollbarSize(Orientation orientation)
{
	if (!scrollbars[orientation].enabled)
		return 0;

	return scrollbars[orientation].size;
}

void ElementScroll::FormatScrollbars()
{
	const Box& element_box = element->GetBox();
	const Vector2f containing_block = element_box.GetSize(BoxArea::Padding);

	for (int i = 0; i < 2; i++)
	{
		if (!scrollbars[i].enabled)
			continue;

		if (i == VERTICAL)
		{
			scrollbars[i].widget->SetBarLength(element->GetClientHeight());
			scrollbars[i].widget->SetTrackLength(element->GetScrollHeight());

			float traversable_track = element->GetScrollHeight() - element->GetClientHeight();
			if (traversable_track > 0)
				scrollbars[i].widget->SetBarPosition(element->GetScrollTop() / traversable_track);
			else
				scrollbars[i].widget->SetBarPosition(0);
		}
		else
		{
			scrollbars[i].widget->SetBarLength(element->GetClientWidth());
			scrollbars[i].widget->SetTrackLength(element->GetScrollWidth());

			float traversable_track = element->GetScrollWidth() - element->GetClientWidth();
			if (traversable_track > 0)
				scrollbars[i].widget->SetBarPosition(element->GetScrollLeft() / traversable_track);
			else
				scrollbars[i].widget->SetBarPosition(0);
		}

		float slider_length = containing_block[1 - i];
		float user_scrollbar_margin = scrollbars[i].element->GetComputedValues().scrollbar_margin();
		float min_scrollbar_margin = GetScrollbarSize(i == VERTICAL ? HORIZONTAL : VERTICAL);
		slider_length -= Math::Max(user_scrollbar_margin, min_scrollbar_margin);

		scrollbars[i].widget->FormatElements(containing_block, slider_length);

		int variable_axis = i == VERTICAL ? 0 : 1;
		Vector2f offset = element_box.GetPosition(BoxArea::Padding);
		offset[variable_axis] += containing_block[variable_axis] -
			(scrollbars[i].element->GetBox().GetSize(BoxArea::Border)[variable_axis] +
				scrollbars[i].element->GetBox().GetEdge(BoxArea::Margin, i == VERTICAL ? BoxEdge::Right : BoxEdge::Bottom));
		// Add the top or left margin (as appropriate) onto the scrollbar's position.
		offset[1 - variable_axis] += scrollbars[i].element->GetBox().GetEdge(BoxArea::Margin, i == VERTICAL ? BoxEdge::Top : BoxEdge::Left);
		scrollbars[i].element->SetOffset(offset, element, true);
	}

	// Format the corner, if it is necessary.
	if (scrollbars[0].enabled && scrollbars[1].enabled)
	{
		CreateCorner();

		Box corner_box;
		LayoutDetails::BuildBox(corner_box, Vector2f(containing_block.x), corner);

		corner_box.SetContent(Vector2f(scrollbars[VERTICAL].size, scrollbars[HORIZONTAL].size));
		corner->SetBox(corner_box);
		corner->SetOffset(containing_block + element_box.GetPosition(BoxArea::Padding) -
				Vector2f(scrollbars[VERTICAL].size, scrollbars[HORIZONTAL].size) - corner_box.GetPosition(BoxArea::Margin),
			element, true);

		corner->SetProperty(PropertyId::Visibility, Property(Style::Visibility::Visible));
	}
}

void ElementScroll::UpdateProperties()
{
	for (Element* scroll_element : {scrollbars[VERTICAL].element, scrollbars[HORIZONTAL].element, corner})
	{
		if (scroll_element)
			UpdateScrollElementProperties(scroll_element);
	}
}

bool ElementScroll::CreateScrollbar(Orientation orientation)
{
	if (scrollbars[orientation].element && scrollbars[orientation].widget)
		return true;

	ElementPtr scrollbar_element =
		Factory::InstanceElement(element, "*", orientation == VERTICAL ? "scrollbarvertical" : "scrollbarhorizontal", XMLAttributes());
	scrollbars[orientation].element = scrollbar_element.get();
	scrollbars[orientation].element->SetProperty(PropertyId::Clip, Property(1, Unit::NUMBER));
	scrollbars[orientation].element->SetProperty(PropertyId::Drag, Property(Style::Drag::Block));

	scrollbars[orientation].widget = MakeUnique<WidgetScroll>(scrollbars[orientation].element);
	scrollbars[orientation].widget->Initialise(orientation == VERTICAL ? WidgetScroll::VERTICAL : WidgetScroll::HORIZONTAL);

	scrollbars[orientation].element->scrollbar_part = ScrollbarPart::Scrollbar;
	if (Element* track = scrollbars[orientation].widget->GetTrack())
		track->scrollbar_part = ScrollbarPart::Track;
	if (Element* bar = scrollbars[orientation].widget->GetBar())
		bar->scrollbar_part = ScrollbarPart::Thumb;

	Element* child = element->AppendChild(std::move(scrollbar_element), false);

	ApplyStandardScrollbarStyle(orientation, HasNonAutoScrollbarProperties(element));
	{
		LayoutNode::ConsumedDirtyScope consumed_dirty_scope;
		UpdateScrollElementProperties(child);
	}

	return true;
}

bool ElementScroll::CreateCorner()
{
	if (corner != nullptr)
		return true;

	ElementPtr corner_element = Factory::InstanceElement(element, "*", "scrollbarcorner", XMLAttributes());
	corner = corner_element.get();
	corner->SetProperty(PropertyId::Clip, Property(1, Unit::NUMBER));
	corner->SetProperty(PropertyId::Drag, Property(Style::Drag::Block));
	corner->scrollbar_part = ScrollbarPart::Corner;

	Element* child = element->AppendChild(std::move(corner_element), false);
	ApplyStandardCornerStyle(HasNonAutoScrollbarProperties(element));
	{
		LayoutNode::ConsumedDirtyScope consumed_dirty_scope;
		UpdateScrollElementProperties(child);
	}

	return true;
}

void ElementScroll::UpdateScrollElementProperties(Element* scroll_element)
{
	// The construction of scrollbars can occur during layouting, then we need some properties and computed values straight away.
	// In particular their size. Furthermore, updating these properties straight away avoids dirtying the document after layouting,
	// which may result in one less additional and unnecessary layouting procedure.

	Context* context = element->GetContext();

	const float dp_ratio = (context ? context->GetDensityIndependentPixelRatio() : 1.0f);
	const Vector2f vp_dimensions = (context ? Vector2f(context->GetDimensions()) : Vector2f(1.0f));
	scroll_element->Update(dp_ratio, vp_dimensions);
}

void ElementScroll::UpdateScrollbarStyle()
{
	const bool standard = HasNonAutoScrollbarProperties(element);
	for (int i = 0; i < 2; i++)
	{
		ApplyStandardScrollbarStyle(Orientation(i), standard);
		if (scrollbars[i].element)
			scrollbars[i].element->DirtyDefinition(Element::DirtyNodes::Self);
	}

	ApplyStandardCornerStyle(standard);
	if (corner)
		corner->DirtyDefinition(Element::DirtyNodes::Self);
}

void ElementScroll::OnDpRatioChange()
{
	if (!HasNonAutoScrollbarProperties(element))
		return;

	for (int i = 0; i < 2; i++)
		ApplyStandardScrollbarStyle(Orientation(i), true);
}

const Element* ElementScroll::GetScrollContainer(const Element* scrollbar_part_element)
{
	switch (scrollbar_part_element->GetScrollbarPart())
	{
	case ScrollbarPart::Scrollbar:
	case ScrollbarPart::Corner: return scrollbar_part_element->GetParentNode();
	case ScrollbarPart::Track:
	case ScrollbarPart::Thumb:
	{
		const Element* scrollbar = scrollbar_part_element->GetParentNode();
		return (scrollbar ? scrollbar->GetParentNode() : nullptr);
	}
	case ScrollbarPart::None: break;
	}
	return nullptr;
}

bool ElementScroll::HasNonAutoScrollbarProperties(const Element* element)
{
	const ElementStyle* style = element->GetStyle();
	const Property* width = style->GetProperty(PropertyId::ScrollbarWidth);
	if (width && width->Get<int>() != 0)
		return true;

	for (const PropertyId id : {PropertyId::ScrollbarThumbColor, PropertyId::ScrollbarTrackColor})
	{
		const Property* color = style->GetProperty(id);
		if (color && (color->unit == Unit::COLOUR || IsCurrentColorKeyword(id, *color)))
			return true;
	}
	return false;
}

void ElementScroll::ApplyStandardScrollbarStyle(Orientation orientation, bool standard)
{
	using namespace ElementScrollFluent;

	Scrollbar& scrollbar = scrollbars[orientation];
	if (!scrollbar.element || !scrollbar.widget)
		return;

	Element* bar = scrollbar.widget->GetBar();
	Element* arrows[2] = {scrollbar.widget->GetArrow(0), scrollbar.widget->GetArrow(1)};
	if (!bar || !arrows[0] || !arrows[1])
		return;

	const bool vertical = (orientation == VERTICAL);
	const PropertyId thickness_id = (vertical ? PropertyId::Width : PropertyId::Height);
	const PropertyId length_id = (vertical ? PropertyId::Height : PropertyId::Width);
	const PropertyId thumb_offset_id = (vertical ? PropertyId::MarginLeft : PropertyId::MarginTop);
	const PropertyId thumb_minimum_id = (vertical ? PropertyId::MinHeight : PropertyId::MinWidth);
	const PropertyId radius_ids[4] = {PropertyId::BorderTopLeftRadius, PropertyId::BorderTopRightRadius, PropertyId::BorderBottomRightRadius,
		PropertyId::BorderBottomLeftRadius};

	if (!standard)
	{
		scrollbar.element->RemoveProperty(thickness_id);
		scrollbar.element->RemoveProperty(PropertyId::BackgroundColor);
		for (Element* arrow : arrows)
		{
			arrow->RemoveProperty(PropertyId::Width);
			arrow->RemoveProperty(PropertyId::Height);
			arrow->RemoveProperty(PropertyId::Decorator);
		}
		bar->RemoveProperty(thickness_id);
		bar->RemoveProperty(thumb_offset_id);
		bar->RemoveProperty(thumb_minimum_id);
		for (const PropertyId id : radius_ids)
			bar->RemoveProperty(id);
		bar->RemoveProperty(PropertyId::BackgroundColor);
		return;
	}

	const Property* width_property = element->GetProperty(PropertyId::ScrollbarWidth);
	const Context* context = element->GetContext();
	const Metrics metrics = GetMetrics(width_property ? width_property->Get<int>() : 0, context ? context->GetDensityIndependentPixelRatio() : 1.f);
	const Colourb track_color = GetColor(element, PropertyId::ScrollbarTrackColor, GetDefaultTrackColor());
	const Colourb thumb_color = GetColor(element, PropertyId::ScrollbarThumbColor, GetDefaultThumbColor());

	scrollbar.element->SetProperty(thickness_id, Property(metrics.thickness, Unit::DEVICE_PX));
	scrollbar.element->SetProperty(PropertyId::BackgroundColor, Property(track_color, Unit::COLOUR));
	for (int i = 0; i < 2; i++)
	{
		arrows[i]->SetProperty(thickness_id, Property(metrics.thickness, Unit::DEVICE_PX));
		arrows[i]->SetProperty(length_id, Property(metrics.button_length, Unit::DEVICE_PX));
		arrows[i]->SetProperty("decorator", GetArrowDecorator(vertical, i == 1, metrics, thumb_color));
	}
	bar->SetProperty(thickness_id, Property(metrics.thumb_thickness, Unit::DEVICE_PX));
	bar->SetProperty(thumb_offset_id, Property(0.5f * (metrics.thickness - metrics.thumb_thickness), Unit::DEVICE_PX));
	bar->SetProperty(thumb_minimum_id, Property(metrics.thumb_minimum_length, Unit::DEVICE_PX));
	for (const PropertyId id : radius_ids)
		bar->SetProperty(id, Property(0.5f * metrics.thumb_thickness, Unit::DEVICE_PX));
	bar->SetProperty(PropertyId::BackgroundColor, Property(thumb_color, Unit::COLOUR));
}

void ElementScroll::ApplyStandardCornerStyle(bool standard)
{
	if (!corner)
		return;

	if (!standard)
	{
		corner->RemoveProperty(PropertyId::BackgroundColor);
		return;
	}

	const Colourb track_color = ElementScrollFluent::GetColor(element, PropertyId::ScrollbarTrackColor, ElementScrollFluent::GetDefaultTrackColor());
	corner->SetProperty(PropertyId::BackgroundColor, Property(track_color, Unit::COLOUR));
}

ElementScroll::Scrollbar::Scrollbar() {}

ElementScroll::Scrollbar::~Scrollbar() {}

} // namespace Rml
