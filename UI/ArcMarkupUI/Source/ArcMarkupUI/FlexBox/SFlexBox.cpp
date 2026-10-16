// Copyright Lukasz Baran. All Rights Reserved.

#include "FlexBox/SFlexBox.h"

#include "Algo/StableSort.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/FlowDirection.h"
#include "Layout/LayoutUtils.h"

SLATE_IMPLEMENT_WIDGET(SFlexBox)
void SFlexBox::PrivateRegisterAttributes(FSlateAttributeInitializer& AttributeInitializer)
{
	FSlateWidgetSlotAttributeInitializer Initializer = SLATE_ADD_PANELCHILDREN_DEFINITION(AttributeInitializer, Children);
	FSlot::RegisterAttributes(Initializer);
}

namespace ArcFlexBoxLayout
{
	/** Resolved per-item layout data, computed once and reused across the arrange passes. */
	struct FFlexItem
	{
		/** Index into SFlexBox::Children. */
		int32 ChildIndex = INDEX_NONE;
		/** flex-basis resolved to a concrete main-axis content size (excludes padding). */
		float Basis = 0.f;
		/** Child's desired main-axis content size (used as flex-basis fallback). */
		float DesiredMain = 0.f;
		// NOTE: a per-slot minimum size could later replace the hard 0 floor if content-clipping becomes a concern.
		/** Shrink floor (default 0 = CSS min-width:0). Items never shrink below this. */
		float MinMain = 0.f;
		/** Total padding along the main axis. */
		float PaddingMain = 0.f;
		/** Total padding along the cross axis. */
		float PaddingCross = 0.f;
		/** Child's desired cross-axis content size (excludes padding). */
		float CrossContent = 0.f;
		float FlexGrow = 0.f;
		float FlexShrink = 1.f;
		/** Resolved main-axis content size after grow/shrink distribution. */
		float ResolvedMain = 0.f;
	};

	/** A wrapped line of flex items (indices into the ordered item array). */
	struct FFlexLine
	{
		int32 StartItem = 0;
		int32 EndItem = 0; // exclusive
		/** Cross extent of the line (max item cross-including-padding). */
		float CrossExtent = 0.f;
		/** Cross-axis position assigned by AlignContent. */
		float CrossOffset = 0.f;
	};

	/** Returns true for the row family (main axis = X). */
	static bool IsRowDirection(EFlexDirection Direction)
	{
		return Direction == EFlexDirection::Row || Direction == EFlexDirection::RowReverse;
	}

	/** Returns true when items run in reverse along the main axis. */
	static bool IsMainReversed(EFlexDirection Direction)
	{
		return Direction == EFlexDirection::RowReverse || Direction == EFlexDirection::ColumnReverse;
	}

	/** Resolve a single child slot into an FFlexItem, given which axis is main. */
	static FFlexItem MakeItem(int32 ChildIndex, const SFlexBox::FSlot& Slot, bool bRowDirection)
	{
		FFlexItem Item;
		Item.ChildIndex = ChildIndex;
		Item.FlexGrow = FMath::Max(0.f, Slot.GetFlexGrow());
		Item.FlexShrink = FMath::Max(0.f, Slot.GetFlexShrink());

		const FMargin SlotPadding = Slot.GetPadding();
		const FVector2f ChildDesired = Slot.GetWidget()->GetDesiredSize();

		if (bRowDirection)
		{
			Item.PaddingMain = SlotPadding.GetTotalSpaceAlong<Orient_Horizontal>();
			Item.PaddingCross = SlotPadding.GetTotalSpaceAlong<Orient_Vertical>();
			Item.DesiredMain = ChildDesired.X;
			Item.CrossContent = ChildDesired.Y;
		}
		else
		{
			Item.PaddingMain = SlotPadding.GetTotalSpaceAlong<Orient_Vertical>();
			Item.PaddingCross = SlotPadding.GetTotalSpaceAlong<Orient_Horizontal>();
			Item.DesiredMain = ChildDesired.Y;
			Item.CrossContent = ChildDesired.X;
		}

		// CSS automatic minimum size (min-width/height: auto): a flex item never shrinks below its
		// min-content size, approximated here by the child's desired main-axis content size. An
		// explicit per-slot MinMain overrides this (clamped >= 0) — set 0 for full collapse (CSS min-*:0).
		const TOptional<float> SlotMin = Slot.GetMinMain();
		Item.MinMain = SlotMin.IsSet() ? FMath::Max(0.f, SlotMin.GetValue()) : Item.DesiredMain;

		const TOptional<float> SlotBasis = Slot.GetFlexBasis();
		Item.Basis = SlotBasis.IsSet() ? FMath::Max(0.f, SlotBasis.GetValue()) : Item.DesiredMain;
		Item.ResolvedMain = Item.Basis;
		return Item;
	}

	/** The outer main-axis size of an item (resolved content + padding). */
	static float ItemOuterMain(const FFlexItem& Item)
	{
		return Item.ResolvedMain + Item.PaddingMain;
	}

	/** The basis-based outer main-axis size of an item (used for grow/shrink free-space math). */
	static float ItemBasisOuterMain(const FFlexItem& Item)
	{
		return Item.Basis + Item.PaddingMain;
	}

	/**
	 * The CSS "hypothetical main size" of an item (basis clamped by the min floor) + padding.
	 * Used for measurement and line-breaking; grow/shrink resolution still runs on the raw
	 * basis so flexBasis:0 + flexGrow patterns keep distributing from zero.
	 */
	static float ItemHypotheticalOuterMain(const FFlexItem& Item)
	{
		return FMath::Max(Item.Basis, Item.MinMain) + Item.PaddingMain;
	}

	/** The outer cross-axis size of an item (cross content + padding). */
	static float ItemOuterCross(const FFlexItem& Item)
	{
		return Item.CrossContent + Item.PaddingCross;
	}

	/**
	 * Distribute free main-axis space across a line of items, writing FFlexItem::ResolvedMain.
	 * Positive free space -> grow by FlexGrow weights. Negative -> iterative shrink-to-min by
	 * (FlexShrink * basis) weights, freezing any item that would drop below its min-content floor.
	 */
	static void ResolveLineMainSizes(TArray<FFlexItem>& Items, int32 StartItem, int32 EndItem, float AvailableMainForItems)
	{
		const int32 ItemCount = EndItem - StartItem;
		if (ItemCount <= 0)
		{
			return;
		}

		float TotalBasisOuter = 0.f;
		for (int32 Index = StartItem; Index < EndItem; ++Index)
		{
			// Reset to basis before resolving (ComputeDesiredSize may have shrunk it previously).
			Items[Index].ResolvedMain = Items[Index].Basis;
			TotalBasisOuter += ItemBasisOuterMain(Items[Index]);
		}

		const float FreeSpace = AvailableMainForItems - TotalBasisOuter;

		if (FreeSpace > UE_KINDA_SMALL_NUMBER)
		{
			// Grow phase: distribute positive free space proportionally to FlexGrow.
			float TotalGrow = 0.f;
			for (int32 Index = StartItem; Index < EndItem; ++Index)
			{
				TotalGrow += Items[Index].FlexGrow;
			}

			if (TotalGrow > UE_KINDA_SMALL_NUMBER)
			{
				for (int32 Index = StartItem; Index < EndItem; ++Index)
				{
					FFlexItem& Item = Items[Index];
					const float Share = FreeSpace * (Item.FlexGrow / TotalGrow);
					Item.ResolvedMain = Item.Basis + Share;
				}
			}
		}
		else if (FreeSpace < -UE_KINDA_SMALL_NUMBER)
		{
			// Shrink phase: iterative fixpoint. Items shrink by (FlexShrink * basis) weights,
			// clamped at their min-content floor; clamped items freeze and their unmet deficit
			// is redistributed among the remaining unfrozen items on the next pass.
			TArray<bool, TInlineAllocator<16>> Frozen;
			Frozen.Init(false, ItemCount);

			// Items that cannot shrink at all are frozen from the start.
			for (int32 Index = StartItem; Index < EndItem; ++Index)
			{
				if (Items[Index].FlexShrink <= UE_KINDA_SMALL_NUMBER)
				{
					Frozen[Index - StartItem] = true;
				}
			}

			const int32 MaxPasses = ItemCount;
			for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
			{
				// Remaining negative space = available - sum of current outer sizes.
				float CurrentTotalOuter = 0.f;
				for (int32 Index = StartItem; Index < EndItem; ++Index)
				{
					CurrentTotalOuter += ItemOuterMain(Items[Index]);
				}
				const float Remaining = AvailableMainForItems - CurrentTotalOuter;

				// Once we've absorbed the deficit (or overflowed into positive), stop.
				if (Remaining >= -UE_KINDA_SMALL_NUMBER)
				{
					break;
				}

				// Sum scaled shrink weights over unfrozen items.
				float TotalScaledShrink = 0.f;
				for (int32 Index = StartItem; Index < EndItem; ++Index)
				{
					if (!Frozen[Index - StartItem])
					{
						TotalScaledShrink += Items[Index].FlexShrink * Items[Index].Basis;
					}
				}

				if (TotalScaledShrink <= UE_KINDA_SMALL_NUMBER)
				{
					// Nothing left that can shrink; the line will overflow.
					break;
				}

				bool bAnyNewFreeze = false;
				for (int32 Index = StartItem; Index < EndItem; ++Index)
				{
					if (Frozen[Index - StartItem])
					{
						continue;
					}

					FFlexItem& Item = Items[Index];
					const float ScaledShrink = Item.FlexShrink * Item.Basis;
					// Remaining is negative; this reduces ResolvedMain.
					const float Adjust = Remaining * (ScaledShrink / TotalScaledShrink);
					const float Target = Item.ResolvedMain + Adjust;

					if (Target <= Item.MinMain + UE_KINDA_SMALL_NUMBER)
					{
						// Would breach the min-content floor: clamp and freeze.
						Item.ResolvedMain = Item.MinMain;
						Frozen[Index - StartItem] = true;
						bAnyNewFreeze = true;
					}
					else
					{
						Item.ResolvedMain = Target;
					}
				}

				if (!bAnyNewFreeze)
				{
					// A full pass with no new freeze means the deficit was fully distributed.
					break;
				}
			}
		}

		// Final guard: never below min-content floor.
		for (int32 Index = StartItem; Index < EndItem; ++Index)
		{
			Items[Index].ResolvedMain = FMath::Max(Items[Index].ResolvedMain, Items[Index].MinMain);
		}
	}

	/** Map a flex cross alignment (resolving Auto against the container default) to an EVerticalAlignment-style int. */
	static EFlexAlign ResolveAlign(EFlexAlign SlotAlign, EFlexAlign ContainerAlign)
	{
		if (SlotAlign == EFlexAlign::Auto)
		{
			return (ContainerAlign == EFlexAlign::Auto) ? EFlexAlign::Stretch : ContainerAlign;
		}
		return SlotAlign;
	}
}

SFlexBox::SFlexBox()
	: Children(this, GET_MEMBER_NAME_CHECKED(SFlexBox, Children))
	, Direction(EFlexDirection::Row)
	, Wrap(EFlexWrap::NoWrap)
	, JustifyContent(EFlexJustify::FlexStart)
	, AlignItems(EFlexAlign::Stretch)
	, AlignContent(EFlexAlign::Stretch)
	, Gap(FVector2f::ZeroVector)
{
	SetCanTick(false);
	bCanSupportFocus = false;
}

SFlexBox::~SFlexBox() = default;

void SFlexBox::Construct(const FArguments& InArgs)
{
	Direction = InArgs._Direction;
	Wrap = InArgs._Wrap;
	JustifyContent = InArgs._JustifyContent;
	AlignItems = InArgs._AlignItems;
	AlignContent = InArgs._AlignContent;
	Gap = InArgs._Gap;

	Children.Reserve(InArgs._Slots.Num());
	for (const FSlot::FSlotArguments& Arg : InArgs._Slots)
	{
		const FSlotBase::FSlotArguments& ChildSlotArgument = static_cast<const FSlotBase::FSlotArguments&>(Arg);
		const FSlot::FSlotArguments& FlexSlotArgument = static_cast<const FSlot::FSlotArguments&>(ChildSlotArgument);
		Children.AddSlot(MoveTemp(const_cast<FSlot::FSlotArguments&>(FlexSlotArgument)));
	}
}

SFlexBox::FScopedWidgetSlotArguments SFlexBox::AddSlot()
{
	return InsertSlot(INDEX_NONE);
}

SFlexBox::FScopedWidgetSlotArguments SFlexBox::InsertSlot(int32 Index)
{
	return FScopedWidgetSlotArguments(MakeUnique<FSlot>(), Children, Index);
}

int32 SFlexBox::RemoveSlot(const TSharedRef<SWidget>& SlotWidget)
{
	return Children.Remove(SlotWidget);
}

void SFlexBox::ClearChildren()
{
	Children.Empty();
}

SFlexBox::FSlot& SFlexBox::GetSlot(int32 SlotIndex)
{
	check(IsValidSlotIndex(SlotIndex));
	return Children[SlotIndex];
}

const SFlexBox::FSlot& SFlexBox::GetSlot(int32 SlotIndex) const
{
	check(IsValidSlotIndex(SlotIndex));
	return Children[SlotIndex];
}

void SFlexBox::SetDirection(EFlexDirection InDirection)
{
	if (Direction != InDirection)
	{
		Direction = InDirection;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SFlexBox::SetWrap(EFlexWrap InWrap)
{
	if (Wrap != InWrap)
	{
		Wrap = InWrap;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SFlexBox::SetJustifyContent(EFlexJustify InJustifyContent)
{
	if (JustifyContent != InJustifyContent)
	{
		JustifyContent = InJustifyContent;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SFlexBox::SetAlignItems(EFlexAlign InAlignItems)
{
	if (AlignItems != InAlignItems)
	{
		AlignItems = InAlignItems;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SFlexBox::SetAlignContent(EFlexAlign InAlignContent)
{
	if (AlignContent != InAlignContent)
	{
		AlignContent = InAlignContent;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

void SFlexBox::SetGap(FVector2f InGap)
{
	if (Gap != InGap)
	{
		Gap = InGap;
		Invalidate(EInvalidateWidgetReason::Layout);
	}
}

FVector2D SFlexBox::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	using namespace ArcFlexBoxLayout;

	const bool bRow = IsRowDirection(Direction);
	const float MainGap = bRow ? Gap.X : Gap.Y;
	const float CrossGap = bRow ? Gap.Y : Gap.X;

	// Build ordered item list (visible only). Order doesn't affect the desired size, but we
	// reuse the same resolution helper for consistency.
	TArray<FFlexItem> Items;
	Items.Reserve(Children.Num());
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FSlot& Slot = Children[ChildIndex];
		if (Slot.GetWidget()->GetVisibility() == EVisibility::Collapsed)
		{
			continue;
		}
		Items.Add(MakeItem(ChildIndex, Slot, bRow));
	}

	if (Items.Num() == 0)
	{
		return FVector2D::ZeroVector;
	}

	if (Wrap == EFlexWrap::NoWrap)
	{
		// Single line: main = sum of hypothetical-outer + inter-item gaps; cross = max item outer cross.
		float MainTotal = 0.f;
		float MaxCross = 0.f;
		for (const FFlexItem& Item : Items)
		{
			MainTotal += ItemHypotheticalOuterMain(Item);
			MaxCross = FMath::Max(MaxCross, ItemOuterCross(Item));
		}
		MainTotal += MainGap * static_cast<float>(Items.Num() - 1);

		return bRow ? FVector2D(MainTotal, MaxCross) : FVector2D(MaxCross, MainTotal);
	}

	// Wrap: simulate line-breaks against this widget's currently-available main extent.
	// ComputeDesiredSize has no allotted geometry; use this widget's last cached local main
	// size as the wrap width (matching how SWrapBox derives its preferred size from content
	// when UseAllottedSize is active). Fall back to the unwrapped total when no size is known.
	const FVector2f LocalSize = GetTickSpaceGeometry().GetLocalSize();
	const float AvailableMain = bRow ? LocalSize.X : LocalSize.Y;

	if (AvailableMain <= UE_KINDA_SMALL_NUMBER)
	{
		// No known width: behave like NoWrap (single line) for the desired-size query.
		float MainTotal = 0.f;
		float MaxCross = 0.f;
		for (const FFlexItem& Item : Items)
		{
			MainTotal += ItemHypotheticalOuterMain(Item);
			MaxCross = FMath::Max(MaxCross, ItemOuterCross(Item));
		}
		MainTotal += MainGap * static_cast<float>(Items.Num() - 1);
		return bRow ? FVector2D(MainTotal, MaxCross) : FVector2D(MaxCross, MainTotal);
	}

	float MaxLineMain = 0.f;
	float TotalCross = 0.f;
	int32 LineCount = 0;

	float LineMain = 0.f;
	float LineCross = 0.f;
	int32 LineItemCount = 0;

	for (const FFlexItem& Item : Items)
	{
		const float ItemMain = ItemHypotheticalOuterMain(Item);
		const float GapBefore = (LineItemCount > 0) ? MainGap : 0.f;

		if (LineItemCount > 0 && (LineMain + GapBefore + ItemMain) > AvailableMain + UE_KINDA_SMALL_NUMBER)
		{
			// Close the current line.
			MaxLineMain = FMath::Max(MaxLineMain, LineMain);
			TotalCross += LineCross;
			++LineCount;

			LineMain = ItemMain;
			LineCross = ItemOuterCross(Item);
			LineItemCount = 1;
		}
		else
		{
			LineMain += GapBefore + ItemMain;
			LineCross = FMath::Max(LineCross, ItemOuterCross(Item));
			++LineItemCount;
		}
	}

	if (LineItemCount > 0)
	{
		MaxLineMain = FMath::Max(MaxLineMain, LineMain);
		TotalCross += LineCross;
		++LineCount;
	}

	if (LineCount > 1)
	{
		TotalCross += CrossGap * static_cast<float>(LineCount - 1);
	}

	return bRow ? FVector2D(MaxLineMain, TotalCross) : FVector2D(TotalCross, MaxLineMain);
}

void SFlexBox::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	using namespace ArcFlexBoxLayout;

	const bool bRow = IsRowDirection(Direction);
	const bool bMainReversed = IsMainReversed(Direction);
	const float MainGap = bRow ? Gap.X : Gap.Y;
	const float CrossGap = bRow ? Gap.Y : Gap.X;

	const FVector2f AllottedSize = AllottedGeometry.GetLocalSize();
	const float AvailableMain = bRow ? AllottedSize.X : AllottedSize.Y;
	const float AvailableCross = bRow ? AllottedSize.Y : AllottedSize.X;

	// 1. Collect visible items and stable-sort by Order (ties keep source order).
	TArray<FFlexItem> Items;
	Items.Reserve(Children.Num());
	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		const FSlot& Slot = Children[ChildIndex];
		if (Slot.GetWidget()->GetVisibility() == EVisibility::Collapsed)
		{
			continue;
		}
		Items.Add(MakeItem(ChildIndex, Slot, bRow));
	}

	if (Items.Num() == 0)
	{
		return;
	}

	// Stable sort by the slot's Order field. Algo::StableSort preserves the relative order of
	// equal elements, satisfying the "ties keep source order" requirement.
	Algo::StableSort(Items, [this](const FFlexItem& A, const FFlexItem& B) -> bool
	{
		return Children[A.ChildIndex].GetOrder() < Children[B.ChildIndex].GetOrder();
	});

	// 2. Break into lines per Wrap, against the available main extent (minus gaps).
	TArray<FFlexLine> Lines;
	if (Wrap == EFlexWrap::NoWrap)
	{
		FFlexLine Line;
		Line.StartItem = 0;
		Line.EndItem = Items.Num();
		Lines.Add(Line);
	}
	else
	{
		int32 LineStart = 0;
		float LineMain = 0.f;
		int32 LineItemCount = 0;
		for (int32 ItemIndex = 0; ItemIndex < Items.Num(); ++ItemIndex)
		{
			const float ItemMain = ItemHypotheticalOuterMain(Items[ItemIndex]);
			const float GapBefore = (LineItemCount > 0) ? MainGap : 0.f;
			if (LineItemCount > 0 && (LineMain + GapBefore + ItemMain) > AvailableMain + UE_KINDA_SMALL_NUMBER)
			{
				FFlexLine Line;
				Line.StartItem = LineStart;
				Line.EndItem = ItemIndex;
				Lines.Add(Line);

				LineStart = ItemIndex;
				LineMain = ItemMain;
				LineItemCount = 1;
			}
			else
			{
				LineMain += GapBefore + ItemMain;
				++LineItemCount;
			}
		}
		FFlexLine Line;
		Line.StartItem = LineStart;
		Line.EndItem = Items.Num();
		Lines.Add(Line);
	}

	// 3. Per line: distribute free main space and compute the line's cross extent.
	for (FFlexLine& Line : Lines)
	{
		const int32 LineItemCount = Line.EndItem - Line.StartItem;
		const float GapsMain = (LineItemCount > 1) ? MainGap * static_cast<float>(LineItemCount - 1) : 0.f;
		const float AvailableForItems = AvailableMain - GapsMain;

		ResolveLineMainSizes(Items, Line.StartItem, Line.EndItem, AvailableForItems);

		float CrossExtent = 0.f;
		for (int32 ItemIndex = Line.StartItem; ItemIndex < Line.EndItem; ++ItemIndex)
		{
			CrossExtent = FMath::Max(CrossExtent, ItemOuterCross(Items[ItemIndex]));
		}
		Line.CrossExtent = CrossExtent;
	}

	// WrapReverse reverses the order in which lines are stacked along the cross axis.
	if (Wrap == EFlexWrap::WrapReverse)
	{
		Algo::Reverse(Lines);
	}

	// 6. AlignContent distributes lines across the cross extent.
	// Single line: treat as a no-op; the line fills the cross extent (CSS behavior).
	{
		float TotalLinesCross = 0.f;
		for (const FFlexLine& Line : Lines)
		{
			TotalLinesCross += Line.CrossExtent;
		}
		const float TotalCrossGaps = (Lines.Num() > 1) ? CrossGap * static_cast<float>(Lines.Num() - 1) : 0.f;
		const float FreeCross = AvailableCross - TotalLinesCross - TotalCrossGaps;

		if (Lines.Num() <= 1)
		{
			// Single line fills the cross extent (so Stretch items can fill the panel).
			if (Lines.Num() == 1)
			{
				Lines[0].CrossOffset = 0.f;
				// Single line: stretch target is the container's cross content size, not
				// max(childDesired, available). An oversized Stretch child clamps to the container and
				// overflows via its own scroll/wrap/clip (CSS single-line cross sizing). Guard the
				// degenerate ~0-cross frame (match the file's epsilon) so children don't flash to zero.
				Lines[0].CrossExtent = (AvailableCross > UE_KINDA_SMALL_NUMBER) ? AvailableCross : Lines[0].CrossExtent;
			}
		}
		else
		{
			const EFlexAlign EffectiveAlignContent = (AlignContent == EFlexAlign::Auto) ? EFlexAlign::Stretch : AlignContent;
			float Cursor = 0.f;
			float BetweenLines = CrossGap;
			float ExtraPerLine = 0.f;

			switch (EffectiveAlignContent)
			{
			case EFlexAlign::FlexStart:
				Cursor = 0.f;
				break;
			case EFlexAlign::FlexEnd:
				Cursor = FMath::Max(0.f, FreeCross);
				break;
			case EFlexAlign::Center:
				Cursor = FMath::Max(0.f, FreeCross) * 0.5f;
				break;
			case EFlexAlign::Stretch:
			default:
				Cursor = 0.f;
				ExtraPerLine = (FreeCross > 0.f) ? (FreeCross / static_cast<float>(Lines.Num())) : 0.f;
				break;
			}

			for (FFlexLine& Line : Lines)
			{
				Line.CrossOffset = Cursor;
				Line.CrossExtent += ExtraPerLine;
				Cursor += Line.CrossExtent + BetweenLines;
			}
		}
	}

	// RTL: for a row container we flip the main-axis direction. Vertical (column) main axes are
	// unaffected by flow direction, consistent with SBoxPanel/SWrapBox.
	const bool bFlowReversesMain = bRow && (GSlateFlowDirection == EFlowDirection::RightToLeft);

	// Loop-invariant: whether the main axis runs in reverse. *Reverse directions reverse the item
	// order; RTL flow also reverses a row. Computed once here because it drives both the
	// JustifyContent origin (below) and the per-item iteration order.
	const bool bReverseItems = bMainReversed != bFlowReversesMain; // XOR

	ArrangedChildren.Reserve(ArrangedChildren.Num() + Items.Num());

	// 4 + 5: position items along main (JustifyContent + gap) and cross (AlignItems/AlignSelf).
	for (const FFlexLine& Line : Lines)
	{
		const int32 LineItemCount = Line.EndItem - Line.StartItem;
		if (LineItemCount <= 0)
		{
			continue;
		}

		// Sum of resolved outer main sizes for this line.
		float UsedMain = 0.f;
		for (int32 ItemIndex = Line.StartItem; ItemIndex < Line.EndItem; ++ItemIndex)
		{
			UsedMain += ItemOuterMain(Items[ItemIndex]);
		}
		const float ExplicitGapsMain = (LineItemCount > 1) ? MainGap * static_cast<float>(LineItemCount - 1) : 0.f;
		const float FreeMain = AvailableMain - UsedMain - ExplicitGapsMain;

		// JustifyContent: leading offset + extra spacing distributed between/around items.
		float Leading = 0.f;
		float BetweenExtra = 0.f; // added on top of the explicit MainGap, between adjacent items
		const float PositiveFree = FMath::Max(0.f, FreeMain);
		switch (JustifyContent)
		{
		case EFlexJustify::FlexStart:
			// A reversed main axis (row-reverse/column-reverse or RTL) flips the main-start edge,
			// so flex-start packs at the far end.
			Leading = bReverseItems ? FreeMain : 0.f;
			break;
		case EFlexJustify::FlexEnd:
			// Mirror of flex-start: reversed main axis moves the main-end edge to the near side.
			Leading = bReverseItems ? 0.f : FreeMain; // may be negative (overflow); items shift past the start edge
			break;
		case EFlexJustify::Center:
			Leading = FreeMain * 0.5f;
			break;
		case EFlexJustify::SpaceBetween:
			Leading = 0.f;
			BetweenExtra = (LineItemCount > 1) ? (PositiveFree / static_cast<float>(LineItemCount - 1)) : 0.f;
			break;
		case EFlexJustify::SpaceAround:
		{
			const float Unit = PositiveFree / static_cast<float>(LineItemCount);
			Leading = Unit * 0.5f;
			BetweenExtra = Unit;
			break;
		}
		case EFlexJustify::SpaceEvenly:
		{
			const float Unit = PositiveFree / static_cast<float>(LineItemCount + 1);
			Leading = Unit;
			BetweenExtra = Unit;
			break;
		}
		default:
			Leading = 0.f;
			break;
		}

		// Build the main-axis offset for each item in this line, in laid-out order.
		// MainCursor accumulates outer sizes + gaps.
		float MainCursor = Leading;

		// Iterate in laid-out order; bReverseItems (computed above) handles *Reverse / RTL.
		for (int32 Step = 0; Step < LineItemCount; ++Step)
		{
			const int32 OrderedStep = bReverseItems ? (LineItemCount - 1 - Step) : Step;
			const int32 ItemIndex = Line.StartItem + OrderedStep;
			const FFlexItem& Item = Items[ItemIndex];

			const float OuterMain = ItemOuterMain(Item);

			// Main-axis local position is the cursor; the child's own main padding is applied
			// inside (we place content after the leading main padding).
			const FSlot& Slot = Children[Item.ChildIndex];
			const FMargin SlotPadding = Slot.GetPadding();

			const float MainPaddingPre = bRow ? SlotPadding.Left : SlotPadding.Top;

			// Cross-axis placement via AlignItems / AlignSelf using AlignChild semantics.
			const EFlexAlign EffectiveCrossAlign = ResolveAlign(Slot.GetAlignSelf(), AlignItems);
			const float LineCrossExtent = Line.CrossExtent;
			const float CrossPaddingTotal = Item.PaddingCross;
			const float CrossPaddingPre = bRow ? SlotPadding.Top : SlotPadding.Left;
			const float CrossPaddingPost = bRow ? SlotPadding.Bottom : SlotPadding.Right;

			float CrossContentSize = 0.f;
			float CrossContentOffset = 0.f;
			switch (EffectiveCrossAlign)
			{
			case EFlexAlign::Stretch:
				CrossContentSize = FMath::Max(0.f, LineCrossExtent - CrossPaddingTotal);
				CrossContentOffset = CrossPaddingPre;
				break;
			case EFlexAlign::FlexStart:
				CrossContentSize = Item.CrossContent;
				CrossContentOffset = CrossPaddingPre;
				break;
			case EFlexAlign::FlexEnd:
				CrossContentSize = Item.CrossContent;
				CrossContentOffset = LineCrossExtent - Item.CrossContent - CrossPaddingPost;
				break;
			case EFlexAlign::Center:
				CrossContentSize = Item.CrossContent;
				CrossContentOffset = (LineCrossExtent - Item.CrossContent - CrossPaddingTotal) * 0.5f + CrossPaddingPre;
				break;
			case EFlexAlign::Auto:
			default:
				CrossContentSize = FMath::Max(0.f, LineCrossExtent - CrossPaddingTotal);
				CrossContentOffset = CrossPaddingPre;
				break;
			}

			const float MainContentPos = MainCursor + MainPaddingPre;
			const float MainContentSize = Item.ResolvedMain;
			const float CrossLinePos = Line.CrossOffset + CrossContentOffset;

			FVector2f LocalPosition;
			FVector2f LocalSize;
			if (bRow)
			{
				LocalPosition = FVector2f(MainContentPos, CrossLinePos);
				LocalSize = FVector2f(MainContentSize, CrossContentSize);
			}
			else
			{
				LocalPosition = FVector2f(CrossLinePos, MainContentPos);
				LocalSize = FVector2f(CrossContentSize, MainContentSize);
			}

			const EVisibility ChildVisibility = Slot.GetWidget()->GetVisibility();
			ArrangedChildren.AddWidget(
				ChildVisibility,
				AllottedGeometry.MakeChild(Slot.GetWidget(), LocalPosition, LocalSize));

			// Advance the cursor by this item's outer main size + explicit gap + justify extra.
			MainCursor += OuterMain;
			if (Step < LineItemCount - 1)
			{
				MainCursor += MainGap + BetweenExtra;
			}
		}
	}
}
