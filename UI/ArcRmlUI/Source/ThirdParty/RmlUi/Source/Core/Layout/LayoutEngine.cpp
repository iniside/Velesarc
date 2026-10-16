#include "LayoutEngine.h"
#include "../ArcPerfInternal.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/Log.h"
#include "../../../Include/RmlUi/Core/Profiling.h"
#include "ContainerBox.h"
#include "FormattingContext.h"
#include "LayoutMeasureCache.h"

namespace Rml {

void LayoutEngine::FormatElement(Element* element, Vector2f containing_block)
{
	RMLUI_ASSERT(element && containing_block.x >= 0 && containing_block.y >= 0);

	LayoutMeasureCache measure_cache;
	RootBox root(containing_block);

	auto layout_box = FormattingContext::FormatIndependent(&root, element, nullptr, FormattingContextType::Block);
	if (!layout_box)
	{
		Log::Message(Log::LT_ERROR, "Error while formatting element: %s", element->GetAddress().c_str());
	}

	{
		RMLUI_ZoneScopedN("ClampScrollOffsetRecursive");
		RMLUI_ARC_PERF_SCOPE(LayoutClampScroll);
		// The size of the scrollable area might have changed, so clamp the scroll offset to avoid scrolling outside the
		// scrollable area. During layouting, we might be changing the scrollable overflow area of the element several
		// times, such as after enabling scrollbars. For this reason, we don't clamp the scroll offset during layouting,
		// as that could inadvertently clamp it to a temporary size. Now that we know the final layout, including the
		// size of each element's scrollable area, we can finally clamp the scroll offset.
		element->ClampScrollOffsetRecursive();
	}
}

void LayoutEngine::FormatOutOfFlowBoundaries(const Vector<Element*>& boundaries)
{
	LayoutMeasureCache measure_cache;

	for (Element* boundary : boundaries)
	{
		RMLUI_ASSERT(boundary->out_of_flow_inputs.placed);
		RMLUI_ARC_PERF_COUNT(LayoutBoundaryFormats, 1);

		// Same containing block as ContainerBox::ClosePositionedElements; the boundary contains all its out-of-flow descendants.
		RootBox root(boundary->out_of_flow_inputs.absolute_containing_block_size);
		UniquePtr<LayoutBox> layout_box = FormattingContext::FormatIndependent(&root, boundary, nullptr, FormattingContextType::Block);
		if (!layout_box)
		{
			Log::Message(Log::LT_ERROR, "Error while formatting element: %s", boundary->GetAddress().c_str());
		}

		boundary->PlaceOutOfFlow(boundary->GetOffsetParent());

		{
			RMLUI_ZoneScopedN("ClampScrollOffsetRecursive");
			RMLUI_ARC_PERF_SCOPE(LayoutClampScroll);
			boundary->ClampScrollOffsetRecursive();
		}
	}
}

#ifdef RMLUI_ARC_PERF
bool LayoutEngine::reference_pass = false;

void LayoutEngine::FormatElementReference(Element* element, Vector2f containing_block)
{
	const bool previous_reference_pass = reference_pass;
	reference_pass = true;
	FormatElement(element, containing_block);
	reference_pass = previous_reference_pass;
}

bool LayoutEngine::IsReferencePass()
{
	return reference_pass;
}
#endif

} // namespace Rml
