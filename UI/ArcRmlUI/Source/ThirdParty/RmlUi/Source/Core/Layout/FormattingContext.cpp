#include "FormattingContext.h"
#include "../ArcPerfInternal.h"
#include "../../../Include/RmlUi/Core/ComputedValues.h"
#include "../../../Include/RmlUi/Core/Element.h"
#include "../../../Include/RmlUi/Core/Profiling.h"
#include "BlockContainer.h"
#include "BlockFormattingContext.h"
#include "ContainerBox.h"
#include "FlexFormattingContext.h"
#include "FloatedBoxSpace.h"
#include "GridFormattingContext.h"
#include "LayoutBox.h"
#include "LayoutDetails.h"
#include "LayoutEngine.h"
#include "LayoutMeasureCache.h"
#include "ReplacedFormattingContext.h"
#include "TableFormattingContext.h"

namespace Rml {

FormattingContext::ShrinkToFitProbeScope* FormattingContext::active_probe_scope = nullptr;

FormattingContext::ShrinkToFitProbeScope::ShrinkToFitProbeScope(Element* in_wrapper) : wrapper(in_wrapper), previous(active_probe_scope)
{
	const ComputedValues& computed = wrapper->GetComputedValues();
	bool active = !LayoutDetails::IsScrollContainer(computed.overflow_x(), computed.overflow_y());
#ifdef RMLUI_ARC_PERF
	active = active && !LayoutEngine::IsReferencePass();
#endif
	if (!active)
		wrapper = nullptr;
	active_probe_scope = this;
}

FormattingContext::ShrinkToFitProbeScope::~ShrinkToFitProbeScope()
{
	active_probe_scope = previous;
}

UniquePtr<LayoutBox> FormattingContext::FormatShrinkToFitProbe(ContainerBox* root, Element* wrapper, const Box* override_initial_box)
{
	{
		ShrinkToFitProbeScope probe_scope(wrapper);
		UniquePtr<LayoutBox> layout_box = FormatIndependent(root, wrapper, override_initial_box, FormattingContextType::Block);
		if (!probe_scope.used_probe_box)
			return layout_box;

		const BlockContainer* block_container =
			(layout_box && layout_box->GetType() == LayoutBox::Type::BlockContainer ? static_cast<const BlockContainer*>(layout_box.get()) : nullptr);
		if (block_container && !block_container->GetBlockBoxSpace()->HasFloats())
			return layout_box;
	}

	return FormatIndependent(root, wrapper, override_initial_box, FormattingContextType::Block);
}

bool FormattingContext::UseShrinkToFitProbeBox(ContainerBox* parent_container, Element* element, const Box* override_initial_box)
{
	Element* wrapper = (active_probe_scope ? active_probe_scope->wrapper : nullptr);
	if (!wrapper || override_initial_box || parent_container->GetElement() != wrapper)
		return false;

	const ComputedValues& computed = element->GetComputedValues();
	const Style::Display display = computed.display();
	const Style::Position position = computed.position();
	const bool probe_box = (display == Style::Display::Flex || display == Style::Display::Grid) && computed.float_() == Style::Float::None &&
		(position == Style::Position::Static || position == Style::Position::Relative) &&
		!LayoutDetails::IsScrollContainer(computed.overflow_x(), computed.overflow_y()) && LayoutDetails::GetBoxParent(element) == wrapper;
	active_probe_scope->used_probe_box |= probe_box;
	return probe_box;
}

bool FormattingContext::UseSizeOnlyFormat(Element* element)
{
	if (!LayoutMeasureCache::IsSizeOnly())
		return false;

	const ComputedValues& computed = element->GetComputedValues();
	const Style::Display display = computed.display();
	return (display == Style::Display::Flex || display == Style::Display::Grid) &&
		!LayoutDetails::IsScrollContainer(computed.overflow_x(), computed.overflow_y());
}

UniquePtr<LayoutBox> FormattingContext::FormatIndependent(ContainerBox* parent_container, Element* element, const Box* override_initial_box,
	FormattingContextType backup_context)
{
	RMLUI_ZoneScopedC(0xAFAFAF);
	using namespace Style;

	RMLUI_ARC_PERF_CODE(ArcPerf::CountFormatCall();)
	if (element->IsReplaced())
	{
		RMLUI_ARC_PERF_ELEMENT_SCOPE(LayoutReplaced, element, LayoutTime);
		return ReplacedFormattingContext::Format(parent_container, element, override_initial_box);
	}

	FormattingContextType type = backup_context;

	auto& computed = element->GetComputedValues();
	const Display display = computed.display();
	Element* box_parent = LayoutDetails::GetBoxParent(element);
	const LayoutMeasureCache::SizeOnlyScope size_only_scope(element, LayoutMeasureCache::IsSizeOnly());
	if (display == Display::Flex || display == Display::InlineFlex)
	{
		type = FormattingContextType::Flex;
	}
	else if (display == Display::Grid || display == Display::InlineGrid)
	{
		type = FormattingContextType::Grid;
	}
	else if (display == Display::Table || display == Display::InlineTable)
	{
		type = FormattingContextType::Table;
	}
	else if (display == Display::InlineBlock || display == Display::FlowRoot || display == Display::TableCell || computed.float_() != Float::None ||
		computed.position() == Position::Absolute || computed.position() == Position::Fixed || computed.overflow_x() != Overflow::Visible ||
		computed.overflow_y() != Overflow::Visible || !box_parent || box_parent->GetDisplay() == Display::Flex ||
		box_parent->GetDisplay() == Display::Grid || box_parent->GetDisplay() == Display::InlineGrid)
	{
		type = FormattingContextType::Block;
	}

	switch (type)
	{
	case FormattingContextType::Block:
	{
		RMLUI_ARC_PERF_ELEMENT_SCOPE(LayoutBlock, element, LayoutTime);
		return BlockFormattingContext::Format(parent_container, element, override_initial_box);
	}
	case FormattingContextType::Table:
	{
		RMLUI_ARC_PERF_ELEMENT_SCOPE(LayoutTable, element, LayoutTime);
		return TableFormattingContext::Format(parent_container, element, override_initial_box);
	}
	case FormattingContextType::Flex:
	{
		RMLUI_ARC_PERF_ELEMENT_SCOPE(LayoutFlex, element, LayoutTime);
		return FlexFormattingContext::Format(parent_container, element, override_initial_box);
	}
	case FormattingContextType::Grid:
	{
		RMLUI_ARC_PERF_ELEMENT_SCOPE(LayoutGrid, element, LayoutTime);
		return GridFormattingContext::Format(parent_container, element, override_initial_box);
	}
	case FormattingContextType::None: break;
	}

	return nullptr;
}

} // namespace Rml
