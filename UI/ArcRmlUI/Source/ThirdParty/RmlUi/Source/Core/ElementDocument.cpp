#include "../../Include/RmlUi/Core/ElementDocument.h"
#include "ArcPerfInternal.h"
#include "../../Include/RmlUi/Core/Context.h"
#include "../../Include/RmlUi/Core/ElementText.h"
#include "../../Include/RmlUi/Core/Factory.h"
#include "../../Include/RmlUi/Core/Profiling.h"
#include "../../Include/RmlUi/Core/StreamMemory.h"
#include "../../Include/RmlUi/Core/StyleSheet.h"
#include "../../Include/RmlUi/Core/StyleSheetContainer.h"
#include "DocumentHeader.h"
#include "ElementStyle.h"
#include "EventDispatcher.h"
#include "Layout/LayoutDetails.h"
#include "Layout/LayoutEngine.h"
#include "Layout/LayoutNode.h"
#include "StyleSheetFactory.h"
#include "Template.h"
#include "TemplateCache.h"
#include "XMLParseTools.h"
#include <algorithm>
#include <limits.h>

namespace Rml {

enum class NavigationSearchDirection { Up, Down, Left, Right };

namespace {
	constexpr int Infinite = INT_MAX;

#ifdef RMLUI_ARC_PERF
	int document_layout_depth = 0;
#endif

	enum class CanFocus { Yes, No, NoAndNoChildren };

	CanFocus CanFocusElement(Element* element)
	{
		if (!element->IsVisible())
			return CanFocus::NoAndNoChildren;

		const ComputedValues& computed = element->GetComputedValues();

		if (computed.focus() == Style::Focus::None)
			return CanFocus::NoAndNoChildren;

		if (computed.tab_index() == Style::TabIndex::Auto)
			return CanFocus::Yes;

		return CanFocus::No;
	}

	bool IsScrollContainer(Element* element)
	{
		const auto& computed = element->GetComputedValues();
		return LayoutDetails::IsScrollContainer(computed.overflow_x(), computed.overflow_y());
	}

	int GetNavigationHeuristic(const Rectanglef& source, const Rectanglef& target, NavigationSearchDirection direction)
	{
		enum Axis { Horizontal = 0, Vertical = 1 };

		auto CalculateHeuristic = [](Axis axis, const Rectanglef& a, const Rectanglef& b) -> int {
			// The heuristic is mainly the distance from the source to the target along the specified direction. In
			// addition, the following factor determines the penalty for being outside the projected area of the element in
			// the given direction, as a multiplier of the cross-axis distance between the target and projected area.
			static constexpr int CrossAxisFactor = 10'000;

			const int main_axis = int(a.p0[axis] - b.p1[axis]);
			if (main_axis < 0)
				return Infinite;

			const Axis cross = Axis((axis + 1) % 2);
			const int cross_axis = Math::Max(0, int(b.p0[cross] - a.p1[cross])) + Math::Max(0, int(a.p0[cross] - b.p1[cross]));

			return main_axis + CrossAxisFactor * cross_axis;
		};

		switch (direction)
		{
		case NavigationSearchDirection::Up: return CalculateHeuristic(Vertical, source, target);
		case NavigationSearchDirection::Down: return CalculateHeuristic(Vertical, target, source);
		case NavigationSearchDirection::Right: return CalculateHeuristic(Horizontal, target, source);
		case NavigationSearchDirection::Left: return CalculateHeuristic(Horizontal, source, target);
		}

		RMLUI_ERROR;
		return Infinite;
	}

	struct SearchNavigationResult {
		Element* element = nullptr;
		int heuristic = Infinite;
	};

	// Search all descendents to determine which element minimizes the navigation heuristic.
	void SearchNavigationTarget(SearchNavigationResult& best_result, Element* element, NavigationSearchDirection direction,
		const Rectanglef& bounding_box, Element* exclude_element)
	{
		const int num_children = element->GetNumChildren();
		for (int child_index = 0; child_index < num_children; child_index++)
		{
			Element* child = element->GetChild(child_index);
			if (child == exclude_element)
				continue;

			const CanFocus can_focus = CanFocusElement(child);
			if (can_focus == CanFocus::Yes)
			{
				const Vector2f position = child->GetAbsoluteOffset(BoxArea::Border);
				const Rectanglef target_box = Rectanglef::FromPositionSize(position, child->GetBox().GetSize(BoxArea::Border));

				const int heuristic = GetNavigationHeuristic(bounding_box, target_box, direction);
				if (heuristic < best_result.heuristic)
				{
					best_result.element = child;
					best_result.heuristic = heuristic;
				}
			}
			else if (can_focus == CanFocus::NoAndNoChildren || IsScrollContainer(child))
			{
				continue;
			}

			SearchNavigationTarget(best_result, child, direction, bounding_box, exclude_element);
		}
	}

} // namespace

RMLUI_RTTI_Define(ElementDocument)

ElementDocument::ElementDocument(const String& tag) : Element(tag)
{
	context = nullptr;

	modal = false;
	focusable_from_modal = false;

	layout_dirty = true;
	position_dirty = false;

	ForceLocalStackingContext();
	SetOwnerDocument(this, true);

	SetProperty(PropertyId::Position, Property(Style::Position::Absolute));
}

ElementDocument::~ElementDocument()
{
	// Once we return from here no further calls should be performed into ElementDocument, even during ~Element. To ensure this,
	// remove this document as owner from this and descendants.
	SetOwnerDocument(nullptr, true);
}

void ElementDocument::ProcessHeader(const DocumentHeader* document_header)
{
	RMLUI_ZoneScoped;
	RMLUI_ARC_PERF_SCOPE(HeaderProcess);

	// Store the source address that we came from
	source_url = document_header->source;

	const DocumentHeader header = MergeDocumentHeaderTemplates(*document_header);

	// Set the title to the document title.
	title = document_header->title;

	// If a style-sheet (or sheets) has been specified for this element, then we load them and set the combined sheet
	// on the element; all of its children will inherit it by default.
	SharedPtr<StyleSheetContainer> new_style_sheet = LoadDocumentHeaderStyleSheet(header);

	// If a style sheet is available, set it on the document.
	if (new_style_sheet)
		SetStyleSheetContainer(std::move(new_style_sheet));

	// Load scripts.
	for (const DocumentHeader::Resource& script : header.scripts)
	{
		if (script.is_inline)
		{
			LoadInlineScript(script.content, script.path, script.line);
		}
		else
		{
			LoadExternalScript(script.path);
		}
	}

	// Hide this document.
	SetProperty(PropertyId::Visibility, Property(Style::Visibility::Hidden));

	const float dp_ratio = (context ? context->GetDensityIndependentPixelRatio() : 1.0f);
	const Vector2f vp_dimensions = (context ? Vector2f(context->GetDimensions()) : Vector2f(1.0f));

	// Update properties so that e.g. visibility status can be queried properly immediately.
	UpdateProperties(dp_ratio, vp_dimensions);
}

Context* ElementDocument::GetContext()
{
	return context;
}

void ElementDocument::SetTitle(const String& _title)
{
	title = _title;
}

const String& ElementDocument::GetTitle() const
{
	return title;
}

const String& ElementDocument::GetSourceURL() const
{
	return source_url;
}

const StyleSheet* ElementDocument::GetStyleSheet() const
{
	if (style_sheet_container)
		return style_sheet_container->GetCompiledStyleSheet();
	return nullptr;
}

const StyleSheetContainer* ElementDocument::GetStyleSheetContainer() const
{
	return style_sheet_container.get();
}

void ElementDocument::SetStyleSheetContainer(SharedPtr<StyleSheetContainer> _style_sheet_container)
{
	RMLUI_ZoneScoped;

	if (style_sheet_container == _style_sheet_container)
		return;

	style_sheet_container = std::move(_style_sheet_container);

	DirtyMediaQueries();
}

void ElementDocument::DirtyMediaQueries()
{
	if (context && style_sheet_container)
	{
		const bool changed_style_sheet = style_sheet_container->UpdateCompiledStyleSheet(context);

		if (changed_style_sheet)
		{
			RMLUI_ARC_PERF_SCOPE(StyleSheetChange);
			DirtyDefinition(Element::DirtyNodes::Self);
			OnStyleSheetChangeRecursive();
		}
	}
}

void ElementDocument::PullToFront()
{
	if (context != nullptr)
		context->PullDocumentToFront(this);
}

void ElementDocument::PushToBack()
{
	if (context != nullptr)
		context->PushDocumentToBack(this);
}

void ElementDocument::Show(ModalFlag modal_flag, FocusFlag focus_flag, ScrollFlag scroll_flag)
{
	switch (modal_flag)
	{
	case ModalFlag::None: modal = false; break;
	case ModalFlag::Modal: modal = true; break;
	case ModalFlag::Keep: break;
	}

	bool focus = false;
	bool autofocus = false;
	bool focus_previous = false;

	switch (focus_flag)
	{
	case FocusFlag::None: break;
	case FocusFlag::Document: focus = true; break;
	case FocusFlag::Keep:
		focus = true;
		focus_previous = true;
		break;
	case FocusFlag::Auto:
		focus = true;
		autofocus = true;
		break;
	}

	// Set to visible and switch focus if necessary.
	SetProperty(PropertyId::Visibility, Property(Style::Visibility::Visible));

	// Update the document now, otherwise the focusing methods below do not think we are visible. This is also important
	// to ensure correct layout for any event handlers, such as for focused input fields to submit the proper caret
	// position.
	UpdateDocument();

	if (focus)
	{
		Element* focus_element = this;

		if (autofocus)
		{
			Element* first_element = nullptr;
			Element* element = FindNextTabElement(this, true);

			while (element && element != first_element)
			{
				if (!first_element)
					first_element = element;

				if (element->HasAttribute("autofocus"))
				{
					focus_element = element;
					break;
				}

				element = FindNextTabElement(element, true);
			}
		}
		else if (focus_previous)
		{
			focus_element = GetFocusLeafNode();
		}

		// Focus the window or element
		bool focused = focus_element->Focus(true);
		if (focused && focus_element != this && scroll_flag == ScrollFlag::Auto)
			focus_element->ScrollIntoView(false);
	}

	DispatchEvent(EventId::Show, Dictionary());
}

void ElementDocument::Hide()
{
	SetProperty(PropertyId::Visibility, Property(Style::Visibility::Hidden));

	// We should update the document now, so that the (un)focusing will get the correct visibility
	UpdateDocument();

	DispatchEvent(EventId::Hide, Dictionary());

	if (context)
	{
		context->UnfocusDocument(this);
	}
}

void ElementDocument::Close()
{
	if (context != nullptr)
		context->UnloadDocument(this);
}

ElementPtr ElementDocument::CreateElement(const String& name)
{
	return Factory::InstanceElement(nullptr, name, name, XMLAttributes());
}

ElementPtr ElementDocument::CreateTextNode(const String& text)
{
	// Create the element.
	ElementPtr element = CreateElement("#text");
	if (!element)
	{
		Log::Message(Log::LT_ERROR, "Failed to create text element, instancer returned nullptr.");
		return nullptr;
	}

	// Cast up
	ElementText* element_text = rmlui_dynamic_cast<ElementText*>(element.get());
	if (!element_text)
	{
		Log::Message(Log::LT_ERROR, "Failed to create text element, instancer didn't return a derivative of ElementText.");
		return nullptr;
	}

	// Set the text
	element_text->SetText(text);

	return element;
}

bool ElementDocument::IsModal() const
{
	return modal && IsVisible();
}

void ElementDocument::LoadInlineScript(const String& /*content*/, const String& /*source_path*/, int /*line*/) {}

void ElementDocument::LoadExternalScript(const String& /*source_path*/) {}

void ElementDocument::UpdateDocument()
{
	const float dp_ratio = (context ? context->GetDensityIndependentPixelRatio() : 1.0f);
	const Vector2f vp_dimensions = (context ? Vector2f(context->GetDimensions()) : Vector2f(1.0f));
	Update(dp_ratio, vp_dimensions);
	RMLUI_ARC_PERF_CODE(if (ArcPerf::active_layout_verify) ArcPerf::VerifyUpdateState(this);)
	UpdateLayout();
	UpdatePosition();
	ElementStyle::ClearSharedStyles();
}

bool ElementDocument::UpdateLayout()
{
	// Note: Carefully consider when to call this function for performance reasons.
	// Ideally, only called once per update loop.
	if (layout_dirty)
	{
		RMLUI_ZoneScoped;
		RMLUI_ZoneText(source_url.c_str(), source_url.size());
		RMLUI_ARC_PERF_SCOPE(DocumentLayout);

		const Vector2f containing_block = GetLayoutContainingBlock();

		RMLUI_ARC_PERF_CODE(document_layout_depth += 1);
		if (FormatDirtyLayout(containing_block))
		{
			Element* first_dirty = nullptr;
			const int num_dirty = LayoutNode::CountDirtyRecursive(this, first_dirty);
			RMLUI_ARC_PERF_COUNT(LayoutResidualPasses, 1);
			RMLUI_ARC_PERF_COUNT(LayoutResidualElements, num_dirty);
			Log::Message(Log::LT_WARNING, "Layout of document '%s' left %d element(s) dirty, first %s; formatting it once more.", source_url.c_str(),
				num_dirty, first_dirty ? first_dirty->GetAddress().c_str() : "(none)");

			if (FormatLayout(containing_block))
			{
				first_dirty = nullptr;
				const int num_still_dirty = LayoutNode::CountDirtyRecursive(this, first_dirty);
				Log::Message(Log::LT_WARNING, "Layout of document '%s' still left %d element(s) dirty after the extra pass, first %s; ignoring them.",
					source_url.c_str(), num_still_dirty, first_dirty ? first_dirty->GetAddress().c_str() : "(none)");
				ClearLayoutDirtyElements();
			}
		}
		layout_dirty = false;

#ifdef RMLUI_ARC_PERF
		if (ArcPerf::active_layout_verify && document_layout_depth == 1)
			VerifyLayoutAgainstReference(true);
		document_layout_depth -= 1;
#endif
		return true;
	}
	return false;
}

void ElementDocument::VerifyUnchangedLayout()
{
#ifdef RMLUI_ARC_PERF
	if (ArcPerf::active_layout_verify && document_layout_depth == 0)
	{
		document_layout_depth += 1;
		VerifyLayoutAgainstReference(false);
		document_layout_depth -= 1;
	}
#endif
}

void ElementDocument::VerifyLayoutAgainstReference(bool layout_ran)
{
#ifdef RMLUI_ARC_PERF
	const Vector2f containing_block = GetLayoutContainingBlock();
	ArcPerf::VerifyLayout(
		this, layout_ran,
		[this]() {
			Element* first_dirty = nullptr;
			return LayoutNode::CountDirtyRecursive(this, first_dirty) + (int)dirty_layout_boundaries.size() + (layout_dirty ? 1 : 0);
		},
		[this, containing_block]() { LayoutEngine::FormatElementReference(this, containing_block); });
	ClearLayoutDirtyElements();
	layout_dirty = false;
#else
	(void)layout_ran;
#endif
}

Vector2f ElementDocument::GetLayoutContainingBlock()
{
	if (Element* parent_node = GetParentNode())
		return parent_node->GetBox().GetSize();
	return Vector2f(0, 0);
}

bool ElementDocument::FormatDirtyLayout(Vector2f containing_block)
{
	Vector<Element*> boundaries;
	if (!CollectOutOfFlowBoundaries(boundaries))
		return FormatLayout(containing_block);

	RMLUI_ARC_PERF_COUNT(LayoutBoundaryPasses, 1);
	ClearLayoutDirtyElements();
	LayoutEngine::FormatOutOfFlowBoundaries(boundaries);
	return !dirty_layout_boundaries.empty();
}

bool ElementDocument::CollectOutOfFlowBoundaries(Vector<Element*>& out_boundaries)
{
	if (dirty_layout_boundaries.empty())
	{
		RMLUI_ARC_PERF_COUNT(LayoutFullNoBoundaryInputs, 1);
		return false;
	}

	Vector<Element*> entries;
	entries.reserve(dirty_layout_boundaries.size());
	for (const ObserverPtr<Element>& dirty_boundary : dirty_layout_boundaries)
	{
		Element* entry = dirty_boundary.get();
		if (!entry || entry->GetOwnerDocument() != this)
			continue;
		if (entry == this)
		{
			RMLUI_ARC_PERF_COUNT(LayoutFullRootDirty, 1);
			return false;
		}
		entries.push_back(entry);
	}

	Vector<Element*> sorted_entries = entries;
	std::sort(sorted_entries.begin(), sorted_entries.end());

	for (Element* entry : entries)
	{
		bool hidden_or_nested = false;
		for (Element* ancestor = entry; ancestor && ancestor != this; ancestor = ancestor->GetParentNode())
		{
			if (ancestor->GetDisplay() == Style::Display::None ||
				(ancestor != entry && std::binary_search(sorted_entries.begin(), sorted_entries.end(), ancestor)))
			{
				hidden_or_nested = true;
				break;
			}
		}
		if (hidden_or_nested)
			continue;

		if (!LayoutNode::IsLayoutBoundary(entry))
		{
			RMLUI_ARC_PERF_COUNT(LayoutFullNoBoundaryInputs, 1);
			return false;
		}
		out_boundaries.push_back(entry);
	}
	return true;
}

bool ElementDocument::FormatLayout(Vector2f containing_block)
{
	RMLUI_ARC_PERF_COUNT(LayoutRuns, 1);
	ClearLayoutDirtyElements();
	LayoutEngine::FormatElement(this, containing_block);
	return !dirty_layout_boundaries.empty();
}

void ElementDocument::ClearLayoutDirtyElements()
{
	LayoutNode::ClearDirtyRecursive(this);
	for (const ObserverPtr<Element>& dirty_boundary : dirty_layout_boundaries)
	{
		if (Element* boundary = dirty_boundary.get())
			LayoutNode::ClearDirtyRecursive(boundary);
	}
	dirty_layout_boundaries.clear();
}

void ElementDocument::UpdatePosition()
{
	if (position_dirty)
	{
		RMLUI_ZoneScoped;
		RMLUI_ARC_PERF_SCOPE(DocumentPosition);

		position_dirty = false;

		Element* root = GetParentNode();

		// We only position ourselves if we are a child of our context's root element. That is, we don't want to proceed
		// if we are unparented or an iframe document.
		if (!root || !context || (root != context->GetRootElement()))
			return;

		// Work out our containing block; relative offsets are calculated against it.
		const Vector2f containing_block = root->GetBox().GetSize();
		auto& computed = GetComputedValues();
		const Box& box = GetBox();

		Vector2f position;

		if (computed.left().type != Style::Left::Auto)
			position.x = ResolveValue(computed.left(), containing_block.x);
		else if (computed.right().type != Style::Right::Auto)
			position.x = containing_block.x - (box.GetSize(BoxArea::Margin).x + ResolveValue(computed.right(), containing_block.x));

		if (computed.top().type != Style::Top::Auto)
			position.y = ResolveValue(computed.top(), containing_block.y);
		else if (computed.bottom().type != Style::Bottom::Auto)
			position.y = containing_block.y - (box.GetSize(BoxArea::Margin).y + ResolveValue(computed.bottom(), containing_block.y));

		// Add the margin edge to the position, since inset properties (top/right/bottom/left) set the margin edge
		// position, while offsets use the border edge.
		position.x += box.GetEdge(BoxArea::Margin, BoxEdge::Left);
		position.y += box.GetEdge(BoxArea::Margin, BoxEdge::Top);

		SetOffset(position, nullptr);
	}
}

void ElementDocument::DirtyPosition()
{
	position_dirty = true;
}

void ElementDocument::DirtyLayoutBoundary(Element* boundary)
{
	layout_dirty = true;
	for (const ObserverPtr<Element>& dirty_boundary : dirty_layout_boundaries)
	{
		if (dirty_boundary.get() == boundary)
			return;
	}
	dirty_layout_boundaries.push_back(boundary->GetObserverPtr());
}

void ElementDocument::DirtyVwAndVhProperties()
{
	GetStyle()->DirtyPropertiesWithUnitsRecursive(Unit::VW | Unit::VH);
}

void ElementDocument::OnPropertyChange(const PropertyIdSet& changed_properties)
{
	Element::OnPropertyChange(changed_properties);

	// If the document's font-size has been changed, we need to dirty all rem properties.
	if (changed_properties.Contains(PropertyId::FontSize))
		GetStyle()->DirtyPropertiesWithUnitsRecursive(Unit::REM);

	if (changed_properties.Contains(PropertyId::Top) ||    //
		changed_properties.Contains(PropertyId::Right) ||  //
		changed_properties.Contains(PropertyId::Bottom) || //
		changed_properties.Contains(PropertyId::Left))
		DirtyPosition();
}

void ElementDocument::ProcessDefaultAction(Event& event)
{
	Element::ProcessDefaultAction(event);

	// Process generic keyboard events for this window in bubble phase
	if (event == EventId::Keydown)
	{
		int key_identifier = event.GetParameter<int>("key_identifier", Input::KI_UNKNOWN);

		// Process TAB
		if (key_identifier == Input::KI_TAB)
		{
			if (Element* element = FindNextTabElement(event.GetTargetElement(), !event.GetParameter<bool>("shift_key", false)))
			{
				if (element->Focus(true))
				{
					element->ScrollIntoView(ScrollAlignment::Adaptive);
					event.StopPropagation();
				}
			}
		}
		// Process direction keys
		else if (key_identifier == Input::KI_LEFT || key_identifier == Input::KI_RIGHT || key_identifier == Input::KI_UP ||
			key_identifier == Input::KI_DOWN)
		{
			NavigationSearchDirection direction = {};
			PropertyId property_id = PropertyId::NavLeft;
			switch (key_identifier)
			{
			case Input::KI_LEFT:
				direction = NavigationSearchDirection::Left;
				property_id = PropertyId::NavLeft;
				break;
			case Input::KI_RIGHT:
				direction = NavigationSearchDirection::Right;
				property_id = PropertyId::NavRight;
				break;
			case Input::KI_UP:
				direction = NavigationSearchDirection::Up;
				property_id = PropertyId::NavUp;
				break;
			case Input::KI_DOWN:
				direction = NavigationSearchDirection::Down;
				property_id = PropertyId::NavDown;
				break;
			}

			auto GetNearestFocusable = [this](Element* focus_node) -> Element* {
				while (focus_node)
				{
					if (CanFocusElement(focus_node) == CanFocus::Yes)
						break;
					focus_node = focus_node->GetParentNode();
				}
				return focus_node ? focus_node : this;
			};
			Element* focus_node = GetNearestFocusable(GetFocusLeafNode());
			if (const Property* nav_property = focus_node->GetLocalProperty(property_id))
			{
				if (Element* next = FindNextNavigationElement(focus_node, direction, *nav_property))
				{
					if (next->Focus(true))
					{
						next->ScrollIntoView(ScrollAlignment::Adaptive);
						event.StopPropagation();
					}
				}
			}
		}
		// Process ENTER being pressed on a focusable object (emulate click)
		else if (key_identifier == Input::KI_RETURN || key_identifier == Input::KI_NUMPADENTER || key_identifier == Input::KI_SPACE)
		{
			Element* focus_node = GetFocusLeafNode();

			if (focus_node && focus_node->GetComputedValues().tab_index() == Style::TabIndex::Auto)
			{
				focus_node->Click();
				event.StopPropagation();
			}
		}
	}
}

void ElementDocument::OnResize()
{
	DirtyPosition();
}

bool ElementDocument::IsFocusableFromModal() const
{
	return focusable_from_modal && IsVisible();
}

void ElementDocument::SetFocusableFromModal(bool focusable)
{
	focusable_from_modal = focusable;
}

Element* ElementDocument::FindNextTabElement(Element* current_element, bool forward, bool wrap_around)
{
	// This algorithm is quite sneaky, I originally thought a depth first search would work, but it appears not. What is
	// required is to cut the tree in half along the nodes from current_element up the root and then either traverse the
	// tree in a clockwise or anticlock wise direction depending if you're searching forward or backward respectively.

	Element* document = current_element->GetOwnerDocument();

	// If we're searching forward, check the immediate children of this node first off. If we're searching backward we
	// only want to consider children if we are the document root.
	if (forward || current_element == document)
	{
		if (Element* result = SearchFocusSubtreeChildren(current_element, forward))
			return result;
	}

	// Now walk up the tree, testing either the bottom or top
	// of the tree, depending on whether we're going forward
	// or backward respectively.
	bool search_enabled = false;
	Element* child = current_element;
	Element* parent = current_element->GetParentNode();
	while (child != document)
	{
		const int num_children = parent->GetNumChildren();
		for (int i = 0; i < num_children; i++)
		{
			// Calculate index into children
			const int child_index = forward ? i : (num_children - i - 1);
			Element* search_child = parent->GetChild(child_index);

			// Do a search if its enabled
			if (search_enabled)
				if (Element* result = SearchFocusSubtree(search_child, forward))
					return result;

			// Enable searching when we reach the child.
			if (search_child == child)
				search_enabled = true;
		}

		// Advance up the tree
		child = parent;
		parent = parent->GetParentNode();
		search_enabled = false;
	}

	// We could not find anything to focus along this direction.
	if (!wrap_around)
		return nullptr;

	// If we can focus the document, then focus that now.
	if (current_element != document && CanFocusElement(document) == CanFocus::Yes)
		return document;

	// Otherwise, search the entire document tree. This way we will wrap around.
	return SearchFocusSubtreeChildren(document, forward);
}

Element* ElementDocument::SearchFocusSubtree(Element* element, bool forward)
{
	CanFocus can_focus = CanFocusElement(element);
	if (can_focus == CanFocus::Yes)
		return element;
	else if (can_focus == CanFocus::NoAndNoChildren)
		return nullptr;

	return SearchFocusSubtreeChildren(element, forward);
}

Element* ElementDocument::SearchFocusSubtreeChildren(Element* element, bool forward)
{
	const int num_children = element->GetNumChildren();
	for (int i = 0; i < num_children; i++)
	{
		const int child_index = (forward ? i : (num_children - i - 1));
		if (Element* result = SearchFocusSubtree(element->GetChild(child_index), forward))
			return result;
	}

	return nullptr;
}

Element* ElementDocument::FindNextNavigationElement(Element* current_element, NavigationSearchDirection direction, const Property& property)
{
	switch (property.unit)
	{
	case Unit::STRING:
	{
		const PropertySource* source = property.source.get();
		const String value = property.Get<String>();
		if (value[0] != '#')
		{
			Log::Message(Log::LT_WARNING,
				"Invalid navigation value '%s': Expected a keyword or a string with an element id prefixed with '#'. Declared at %s:%d",
				value.c_str(), source ? source->path.c_str() : "", source ? source->line_number : -1);
			return nullptr;
		}

		const String id = String(value.begin() + 1, value.end());
		Element* result = GetElementById(id);
		if (!result)
		{
			Log::Message(Log::LT_WARNING, "Trying to navigate to element with id '%s', but could not find element. Declared at %s:%d", id.c_str(),
				source ? source->path.c_str() : "", source ? source->line_number : -1);
		}
		return result;
	}
	break;
	case Unit::KEYWORD:
	{
		const bool direction_is_horizontal = (direction == NavigationSearchDirection::Left || direction == NavigationSearchDirection::Right);
		const bool direction_is_vertical = (direction == NavigationSearchDirection::Up || direction == NavigationSearchDirection::Down);
		const bool direction_is_forward = (direction == NavigationSearchDirection::Down || direction == NavigationSearchDirection::Right);
		switch (static_cast<Style::Nav>(property.value.Get<int>()))
		{
		case Style::Nav::None: return nullptr;
		case Style::Nav::Auto: break;
		case Style::Nav::Horizontal:
			if (!direction_is_horizontal)
				return nullptr;
			break;
		case Style::Nav::Vertical:
			if (!direction_is_vertical)
				return nullptr;
			break;
		case Style::Nav::TreeOrder: return FindNextTabElement(current_element, direction_is_forward, false);
		}

		if (current_element == this)
			return FindNextTabElement(current_element, direction_is_forward);

		const Vector2f position = current_element->GetAbsoluteOffset(BoxArea::Border);
		const Rectanglef bounding_box = Rectanglef::FromPositionSize(position, current_element->GetBox().GetSize(BoxArea::Border));

		auto GetNearestScrollContainer = [this](Element* element) -> Element* {
			for (element = element->GetParentNode(); element; element = element->GetParentNode())
			{
				if (IsScrollContainer(element))
					return element;
			}
			return this;
		};
		Element* start_element = GetNearestScrollContainer(current_element);

		SearchNavigationResult best_result;
		SearchNavigationTarget(best_result, start_element, direction, bounding_box, current_element);
		return best_result.element;
	}
	break;
	default: break;
	}
	return nullptr;
}

} // namespace Rml
