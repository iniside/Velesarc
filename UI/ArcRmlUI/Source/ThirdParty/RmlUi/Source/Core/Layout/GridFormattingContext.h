#pragma once

#include "../../../Include/RmlUi/Core/Types.h"
#include "FormattingContext.h"
#include "GridLayoutTypes.h"

namespace Rml {

class LayoutBox;
class ContainerBox;
class GridContainer;

/*
    Formats a grid container element and its grid items according to the CSS grid layout rules, using a port of the Taffy grid algorithm.
*/
class GridFormattingContext final : public FormattingContext, public GridItemMeasurer {
public:
	/// Formats a grid container element and its grid items.
	static UniquePtr<LayoutBox> Format(ContainerBox* parent_container, Element* element, const Box* override_initial_box);

	/// Computes the max-content width of a grid container.
	static float GetMaxContentWidth(Element* element);
	/// Computes the min-content width of a grid container, sizing its columns under a min-content constraint.
	static float GetMinContentWidth(Element* element);

	float MeasureChildSize(const GridItem& item, GridAxis axis, GridOptionalSize known_dimensions, GridOptionalSize parent_size,
		GridAvailableSize available_space) const override;
	void MeasureChildBaseline(const GridItem& item, GridOptionalSize parent_size, float& out_baseline, float& out_height) const override;

private:
	GridFormattingContext() = default;

	/// Format the grid and its items.
	/// @param[out] grid_resulting_content_size The final content size of the grid container.
	/// @param[out] grid_visible_overflow_size Border-box overflow in case grid items or their contents overflow the container.
	/// @param[out] grid_scrollable_content_size Margin-box overflow of the grid items, contributing only to the container's scrollable overflow.
	/// @param[out] grid_baseline The baseline of the grid container, in terms of the vertical distance from its top-left border corner.
	void Format(Vector2f& grid_resulting_content_size, Vector2f& grid_visible_overflow_size, Vector2f& grid_scrollable_content_size,
		float& grid_baseline) const;

	static float ComputeMaxContentWidth(Element* element);
	static float ComputeMinContentWidth(Element* element);
	static float ComputeIntrinsicWidth(Element* element, GridAvailableSpace indefinite_width);
	static float GetItemContentWidth(Element* element, const Box& box, Vector2f containing_block, GridAvailableSpace available_width);
	static float ComputeItemContentWidth(Element* element, const Box& box, Vector2f containing_block, GridAvailableSpace available_width);

	Vector2f grid_available_content_size;
	Vector2f grid_content_containing_block;
	Vector2f grid_content_offset;
	Vector2f grid_min_size;
	Vector2f grid_max_size;
	GridAvailableSpace indefinite_width = GridAvailableSpace::MaxContent();
	bool compute_width_only = false;
	bool size_only = false;

	Element* element_grid = nullptr;
	GridContainer* grid_container_box = nullptr;
};

} // namespace Rml
