#pragma once

#include "../../../Include/RmlUi/Core/Types.h"
#include <stdint.h>

namespace Rml {

class Element;
class PropertyIdSet;

/// Per-element layout dirty state. Self marks the element's own layout as changed; Child marks an ancestor of a Self-dirty element.
class LayoutNode {
public:
	enum class DirtyType : uint8_t { Structural, BoxOnly };

	/// Marks the element Self dirty and its ancestors Child dirty up to the nearest layout boundary, stopping at the first ancestor
	/// already marked. The boundary reached is added to the owner document's set of boundaries needing a format. A box-only
	/// change of a boundary marks only the boundary itself.
	static void DirtyLayout(Element* element, DirtyType type);

	/// Returns BoxOnly if every changed layout-forcing property only sizes or insets the element's box.
	static DirtyType ClassifyPropertyChange(const PropertyIdSet& changed_properties_forcing_layout);

	/// The document, or an absolutely or fixed positioned element with a formatting context that a containing block has placed
	/// from its stored out-of-flow inputs.
	static bool IsLayoutBoundary(const Element* element);

	/// Clears the dirty state of the element and of every dirty element below it, stopping at clean elements.
	static void ClearDirtyRecursive(Element* element);
	/// Returns the number of dirty elements at and below the element, visiting every element, and the first Self-dirty one in tree order.
	static int CountDirtyRecursive(Element* element, Element*& first_self_dirty);

	bool IsDirty() const { return self_dirty || child_dirty; }
	bool IsSelfDirty() const { return self_dirty; }
	bool IsChildDirty() const { return child_dirty; }
	bool IsBoxOnlyDirty() const { return self_dirty && self_box_only; }
	void ClearDirty();

	/// Drops the marks made while alive, for changes the running layout pass formats afterwards.
	class ConsumedDirtyScope {
	public:
		ConsumedDirtyScope();
		~ConsumedDirtyScope();
		ConsumedDirtyScope(const ConsumedDirtyScope&) = delete;
		ConsumedDirtyScope& operator=(const ConsumedDirtyScope&) = delete;
	};

private:
	bool self_dirty = false;
	bool self_box_only = false;
	bool child_dirty = false;

	static int consumed_dirty_scope_depth;
};

} // namespace Rml
