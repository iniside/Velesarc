#include "LayoutNode.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/ElementDocument.h"
#include "../../../Include/RmlUi/Core/PropertyIdSet.h"
#include "../ArcPerfInternal.h"
#include "../ElementMeta.h"

namespace Rml {

int LayoutNode::consumed_dirty_scope_depth = 0;

void LayoutNode::DirtyLayout(Element* element, DirtyType type)
{
	RMLUI_ARC_PERF_CODE({
		RMLUI_ARC_PERF_COUNT(LayoutDirtyCalls, 1);
		if (ArcPerf::Detailed())
			ArcPerf::RecordElementDetail(ArcPerf::ElementList::LayoutDirty, element, String("call in ") + ArcPerf::GetCurrentBucketName());
	})

	ElementDocument* document = element->GetOwnerDocument();
	if (!document || consumed_dirty_scope_depth > 0)
		return;

	LayoutNode& node = element->meta->layout_node;
	if (!node.self_dirty)
		node.self_box_only = (type == DirtyType::BoxOnly);
	else if (type == DirtyType::Structural)
		node.self_box_only = false;
	node.self_dirty = true;

	// A box-only change of an out-of-flow boundary is formatted from its stored inputs; its result never feeds its ancestors.
	Element* boundary = element;
	const bool stops_at_self = IsLayoutBoundary(element) && (type == DirtyType::BoxOnly || element->GetOwnerDocument() == element);
	if (!stops_at_self)
	{
		boundary = nullptr;
		for (Element* ancestor = element->GetParentNode(); ancestor; ancestor = ancestor->GetParentNode())
		{
			LayoutNode& ancestor_node = ancestor->meta->layout_node;
			const bool already_marked = ancestor_node.IsDirty();
			ancestor_node.child_dirty = true;
			if (already_marked)
				break;
			if (IsLayoutBoundary(ancestor))
			{
				boundary = ancestor;
				break;
			}
		}
	}

	if (boundary)
		document->DirtyLayoutBoundary(boundary);
}

LayoutNode::DirtyType LayoutNode::ClassifyPropertyChange(const PropertyIdSet& changed_properties_forcing_layout)
{
	static const PropertyIdSet box_properties = []() {
		PropertyIdSet set;
		for (PropertyId id : {PropertyId::Width, PropertyId::MinWidth, PropertyId::MaxWidth, PropertyId::Height, PropertyId::MinHeight,
				 PropertyId::MaxHeight, PropertyId::Top, PropertyId::Right, PropertyId::Bottom, PropertyId::Left, PropertyId::MarginTop,
				 PropertyId::MarginRight, PropertyId::MarginBottom, PropertyId::MarginLeft, PropertyId::PaddingTop, PropertyId::PaddingRight,
				 PropertyId::PaddingBottom, PropertyId::PaddingLeft, PropertyId::BorderTopWidth, PropertyId::BorderRightWidth,
				 PropertyId::BorderBottomWidth, PropertyId::BorderLeftWidth, PropertyId::BoxSizing})
			set.Insert(id);
		return set;
	}();

	for (PropertyId id : changed_properties_forcing_layout)
	{
		if (!box_properties.Contains(id))
			return DirtyType::Structural;
	}
	return DirtyType::BoxOnly;
}

bool LayoutNode::IsLayoutBoundary(const Element* element)
{
	if (element->GetOwnerDocument() == element)
		return true;

	using namespace Style;
	const ComputedValues& computed = element->GetComputedValues();
	if (computed.position() != Position::Absolute && computed.position() != Position::Fixed)
		return false;
	if (computed.display() == Display::None || computed.display() == Display::Contents)
		return false;
	return element->out_of_flow_inputs.placed;
}

void LayoutNode::ClearDirtyRecursive(Element* element)
{
	LayoutNode& node = element->meta->layout_node;
	if (!node.IsDirty())
		return;
	node.ClearDirty();

	const int num_children = element->GetNumChildren(true);
	for (int i = 0; i < num_children; i++)
		ClearDirtyRecursive(element->GetChild(i));
}

int LayoutNode::CountDirtyRecursive(Element* element, Element*& first_self_dirty)
{
	const LayoutNode& node = element->meta->layout_node;
	if (node.self_dirty && !first_self_dirty)
		first_self_dirty = element;

	int count = node.IsDirty() ? 1 : 0;
	const int num_children = element->GetNumChildren(true);
	for (int i = 0; i < num_children; i++)
		count += CountDirtyRecursive(element->GetChild(i), first_self_dirty);
	return count;
}

void LayoutNode::ClearDirty()
{
	self_dirty = false;
	self_box_only = false;
	child_dirty = false;
}

LayoutNode::ConsumedDirtyScope::ConsumedDirtyScope()
{
	consumed_dirty_scope_depth += 1;
}

LayoutNode::ConsumedDirtyScope::~ConsumedDirtyScope()
{
	consumed_dirty_scope_depth -= 1;
}

} // namespace Rml
