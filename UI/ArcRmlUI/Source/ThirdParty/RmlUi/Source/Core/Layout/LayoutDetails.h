#pragma once

#include "../../../Include/RmlUi/Core/StyleTypes.h"
#include "../../../Include/RmlUi/Core/Types.h"

namespace Rml {

class Box;
class BlockContainer;
class ContainerBox;

/**
    ComputedAxisSize is an abstraction of an element's computed size properties along a single axis, either horizontally or vertically,
    allowing eg. rows and columns alike to use the same algorithms. Here, 'a' means left or top, 'b' means right or bottom.
*/
struct ComputedAxisSize {
	Style::LengthPercentageAuto size;
	Style::LengthPercentage min_size, max_size;
	Style::Padding padding_a, padding_b;
	Style::Margin margin_a, margin_b;
	float border_a, border_b;
	Style::BoxSizing box_sizing;
};

struct ContainingBlock {
	ContainerBox* container;
	Vector2f size;
};

enum class BuildBoxMode {
	Block,          // Sets edges and size if available, auto width can result in shrink-to-fit width, auto margins are used for alignment.
	Inline,         // Sets edges, ignores width, height, and auto margins.
	UnalignedBlock, // Like block, but auto width returns -1, and auto margins are resolved to zero.
};

/**
    Layout functions for sizing elements.

    Some procedures based on the CSS 2.1 specification, 'Section 10. Visual formatting model details'.
 */
class LayoutDetails {
public:
	/// Generates the box dimensions for an element.
	/// @param[out] box The box to be built.
	/// @param[in] containing_block The dimensions of the content area of the block containing the element.
	/// @param[in] element The element to build the box for.
	/// @param[in] box_context The formatting context in which the box is generated.
	static void BuildBox(Box& box, Vector2f containing_block, Element* element, BuildBoxMode box_context = BuildBoxMode::Block);

	// Generates the box of a scrollbar element, with auto sizes resolved to zero and then clamped by the min and max size properties.
	static void BuildScrollbarBox(Box& box, Vector2f containing_block, Element* element);

	// Retrieves the minimum and maximum width from an element's computed values.
	static void GetMinMaxWidth(float& min_width, float& max_width, const ComputedValues& computed, const Box& box, float containing_block_width);

	// Retrieves the minimum and maximum height from an element's computed values.
	static void GetMinMaxHeight(float& min_height, float& max_height, const ComputedValues& computed, const Box& box, float containing_block_height);

	// Retrieves the minimum and maximum height, set to the box's content height if it is definite (>= 0), otherwise retrieves the minimum and maximum
	// heights from an element's computed values.
	static void GetDefiniteMinMaxHeight(float& min_height, float& max_height, const ComputedValues& computed, const Box& box,
		float containing_block_height);

	/// Returns the containing block for a box.
	/// @param[in] parent_container The parent container of the current box.
	/// @param[in] position The position property of the current box.
	/// @return The containing block box and size, possibly indefinite (represented by negative size) along one or both axes.
	static ContainingBlock GetContainingBlock(ContainerBox* parent_container, Style::Position position);

	/// Builds margins of a Box, and resolves any auto width or height for non-inline elements. The height may be left unresolved if it depends on the
	/// element's children.
	/// @param[in,out] box The box to generate. The padding and borders, in addition to any definite content area, must be set on the box already.
	/// Auto width and height are specified by negative content size.
	/// @param[in] min_size The element's minimum width and height.
	/// @param[in] max_size The element's maximum width and height.
	/// @param[in] containing_block The size of the containing block.
	/// @param[in] element The element the box is being generated for.
	/// @param[in] box_context The formatting context in which the box is generated.
	/// @param[in] replaced_element True when the element is a replaced element.
	static void BuildBoxSizeAndMargins(Box& box, Vector2f min_size, Vector2f max_size, Vector2f containing_block, Element* element,
		BuildBoxMode box_context, bool replaced_element);

	/// Formats the element and returns the width of its contents.
	static float GetShrinkToFitWidth(Element* element, Vector2f containing_block);

	/// Returns true for an element that establishes a flex or grid formatting context, with its fit-content width in out_width: the
	/// max-content width when it fits the available content width or that width is negative, else max(min(min-content, max-content), available).
	static bool GetFlexOrGridFitContentWidth(Element* element, float available_width, float& out_width);

	/// Returns the content width of the element under a min-content constraint; the box is built for the containing block.
	static float GetMinContentWidth(Element* element, const Box& box, Vector2f containing_block);

	/// Build computed axis size along the horizontal direction (width and friends).
	static ComputedAxisSize BuildComputedHorizontalSize(const ComputedValues& computed);
	/// Build computed axis size along the vertical direction (height and friends).
	static ComputedAxisSize BuildComputedVerticalSize(const ComputedValues& computed);
	/// Resolve edge sizes from a computed axis size.
	static void GetEdgeSizes(float& margin_a, float& margin_b, float& padding_border_a, float& padding_border_b,
		const ComputedAxisSize& computed_size, float base_value);

	/// Returns the preferred aspect ratio (width over height) of an element, or zero when it has none. For replaced elements, an 'auto' ratio
	/// resolves to the intrinsic ratio when there is one.
	static float GetAspectRatio(const ComputedValues& computed, bool replaced_element, float intrinsic_ratio);
	/// Returns the padding and border sums that the aspect ratio includes, which are zero for the content-box sizing model.
	static Vector2f GetAspectRatioEdges(const ComputedValues& computed, const Box& box);
	/// Transfers a content width or height to the other axis through an aspect ratio, results are never negative.
	static float ContentHeightFromWidth(float content_width, float ratio, Vector2f edges);
	static float ContentWidthFromHeight(float content_height, float ratio, Vector2f edges);

	static String GetDebugElementName(Element* element);

	/// Returns true for a text run made only of white space inside a flex or grid container, which generates no anonymous item. Its lines
	/// from any previous layout are cleared.
	static bool SkipWhiteSpaceTextItem(Element* element);

	/// Appends the layout children of an element, replacing each 'display: contents' child by its own layout children, recursively.
	static void CollectBoxChildren(Element* element, Element* offset_parent, Vector<Element*>& out_children);
	/// Gives an element with 'display: contents', which generates no box, an empty box at the origin of its offset parent.
	static void PlaceContentsElement(Element* element, Element* offset_parent);
	/// Returns the closest ancestor that generates a box, skipping 'display: contents' ancestors.
	static Element* GetBoxParent(Element* element);

	static bool IsScrollContainer(Style::Overflow overflow_x, Style::Overflow overflow_y)
	{
		return overflow_x != Style::Overflow::Visible || overflow_y != Style::Overflow::Visible;
	}

private:
	static float ComputeShrinkToFitWidth(Element* element, Vector2f containing_block);

	/// Calculates and returns the content size for replaced elements.
	static Vector2f CalculateSizeForReplacedElement(Vector2f specified_content_size, Vector2f min_size, Vector2f max_size, Vector2f intrinsic_size,
		float intrinsic_ratio);

	/// Builds the block-specific width and horizontal margins of a Box.
	/// @param[in,out] box The box to generate. The padding and borders must be set on the box already. The content area is used instead of the width
	/// property, and -1 means auto width.
	/// @param[in] computed The computed values of the element the box is being generated for.
	/// @param[in] min_width The minimum content width of the element.
	/// @param[in] max_width The maximum content width of the element.
	/// @param[in] containing_block The size of the containing block.
	/// @param[in] element The element the box is being generated for.
	/// @param[in] replaced_element True when the element is a replaced element.
	/// @param[in] override_shrink_to_fit_width Provide a fixed shrink-to-fit width instead of formatting the element when its properties allow
	/// shrinking.
	static void BuildBoxWidth(Box& box, const ComputedValues& computed, float min_width, float max_width, Vector2f containing_block, Element* element,
		bool replaced_element, float override_shrink_to_fit_width = -1);
	/// Builds the block-specific height and vertical margins of a Box.
	/// @param[in,out] box The box to generate. The padding and borders must be set on the box already. The content area is used instead of the height
	/// property, and -1 means auto height.
	/// @param[in] computed The computed values of the element the box is being generated for.
	/// @param[in] min_height The minimum content height of the element.
	/// @param[in] max_height The maximum content height of the element.
	/// @param[in] containing_block_height The height of the containing block.
	static void BuildBoxHeight(Box& box, const ComputedValues& computed, float min_height, float max_height, float containing_block_height);
};

} // namespace Rml
