#pragma once

#include "../../../Include/RmlUi/Core/Types.h"
#include "FormattingContext.h"

namespace Rml {

class LayoutBox;
class ContainerBox;
class FlexContainer;

/*
    Formats a flex container element and its flex items according to flexible box (flexbox) layout rules.
*/
class FlexFormattingContext final : public FormattingContext {
public:
	/// Formats a flex container element and its flex items according to flexbox layout rules.
	static UniquePtr<LayoutBox> Format(ContainerBox* parent_container, Element* element, const Box* override_initial_box);

	/// Computes the max-content width of a flex container.
	static float GetMaxContentWidth(Element* element);
	/// Computes the min-content width of a flex container from its items' min-content contributions.
	static float GetMinContentWidth(Element* element);

private:
	FlexFormattingContext() = default;

	static float ComputeMaxContentWidth(Element* element);
	static float ComputeMinContentWidth(Element* element);

	/// Format the flexbox and its children.
	/// @param[out] flex_resulting_content_size The final content size of the flex container.
	/// @param[out] flex_visible_overflow_size Border-box overflow in case flex items or their contents overflow the container.
	/// @param[out] flex_scrollable_content_size Margin-box overflow of the flex items, contributing only to the container's scrollable overflow.
	/// @param[out] flex_baseline The baseline of the flex container, in terms of the vertical distance from its top-left border corner.
	void Format(Vector2f& flex_resulting_content_size, Vector2f& flex_visible_overflow_size, Vector2f& flex_scrollable_content_size,
		float& flex_baseline) const;

	Vector2f flex_available_content_size;
	Vector2f flex_content_containing_block;
	Vector2f flex_content_offset;
	Vector2f flex_min_size;
	Vector2f flex_max_size;
	bool compute_width_only = false;
	bool size_only = false;

	Element* element_flex = nullptr;
	FlexContainer* flex_container_box = nullptr;
};

} // namespace Rml
