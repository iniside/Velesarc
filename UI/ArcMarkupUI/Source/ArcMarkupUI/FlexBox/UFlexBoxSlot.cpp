// Copyright Lukasz Baran. All Rights Reserved.

#include "FlexBox/UFlexBoxSlot.h"
#include "FlexBox/SFlexBox.h"
#include "Components/Widget.h"
#include "Widgets/SNullWidget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UFlexBoxSlot)

// Enum-order static asserts — if SFlexBox.h reorders a Slate enum this will break at compile time.
static_assert((int32)EArcFlexDirection::Row          == (int32)EFlexDirection::Row,         "EArcFlexDirection::Row mapping drifted");
static_assert((int32)EArcFlexDirection::RowReverse   == (int32)EFlexDirection::RowReverse,  "EArcFlexDirection::RowReverse mapping drifted");
static_assert((int32)EArcFlexDirection::Column       == (int32)EFlexDirection::Column,      "EArcFlexDirection::Column mapping drifted");
static_assert((int32)EArcFlexDirection::ColumnReverse== (int32)EFlexDirection::ColumnReverse,"EArcFlexDirection::ColumnReverse mapping drifted");

static_assert((int32)EArcFlexWrap::NoWrap      == (int32)EFlexWrap::NoWrap,     "EArcFlexWrap::NoWrap mapping drifted");
static_assert((int32)EArcFlexWrap::Wrap        == (int32)EFlexWrap::Wrap,       "EArcFlexWrap::Wrap mapping drifted");
static_assert((int32)EArcFlexWrap::WrapReverse == (int32)EFlexWrap::WrapReverse,"EArcFlexWrap::WrapReverse mapping drifted");

static_assert((int32)EArcFlexJustify::FlexStart   == (int32)EFlexJustify::FlexStart,   "EArcFlexJustify::FlexStart mapping drifted");
static_assert((int32)EArcFlexJustify::FlexEnd     == (int32)EFlexJustify::FlexEnd,     "EArcFlexJustify::FlexEnd mapping drifted");
static_assert((int32)EArcFlexJustify::Center      == (int32)EFlexJustify::Center,      "EArcFlexJustify::Center mapping drifted");
static_assert((int32)EArcFlexJustify::SpaceBetween== (int32)EFlexJustify::SpaceBetween,"EArcFlexJustify::SpaceBetween mapping drifted");
static_assert((int32)EArcFlexJustify::SpaceAround == (int32)EFlexJustify::SpaceAround, "EArcFlexJustify::SpaceAround mapping drifted");
static_assert((int32)EArcFlexJustify::SpaceEvenly == (int32)EFlexJustify::SpaceEvenly, "EArcFlexJustify::SpaceEvenly mapping drifted");

static_assert((int32)EArcFlexAlign::Auto     == (int32)EFlexAlign::Auto,     "EArcFlexAlign::Auto mapping drifted");
static_assert((int32)EArcFlexAlign::FlexStart== (int32)EFlexAlign::FlexStart,"EArcFlexAlign::FlexStart mapping drifted");
static_assert((int32)EArcFlexAlign::FlexEnd  == (int32)EFlexAlign::FlexEnd,  "EArcFlexAlign::FlexEnd mapping drifted");
static_assert((int32)EArcFlexAlign::Center   == (int32)EFlexAlign::Center,   "EArcFlexAlign::Center mapping drifted");
static_assert((int32)EArcFlexAlign::Stretch  == (int32)EFlexAlign::Stretch,  "EArcFlexAlign::Stretch mapping drifted");

namespace UFlexBoxSlotPrivate
{
	EFlexDirection ToSlate(EArcFlexDirection In)
	{
		return static_cast<EFlexDirection>(In);
	}

	EFlexWrap ToSlate(EArcFlexWrap In)
	{
		return static_cast<EFlexWrap>(In);
	}

	EFlexJustify ToSlate(EArcFlexJustify In)
	{
		return static_cast<EFlexJustify>(In);
	}

	EFlexAlign ToSlate(EArcFlexAlign In)
	{
		return static_cast<EFlexAlign>(In);
	}

	/** Shared mapping for the "<0 = auto/unset" float properties (FlexBasis, MinMainSize). */
	TOptional<float> NegativeToUnset(float InValue)
	{
		if (InValue < 0.f)
		{
			return TOptional<float>();
		}
		return TOptional<float>(InValue);
	}
}

/////////////////////////////////////////////////////
// UFlexBoxSlot

void UFlexBoxSlot::BuildSlot(TSharedRef<SFlexBox> InFlexBox)
{
	using namespace UFlexBoxSlotPrivate;

	InFlexBox->AddSlot()
		.Expose(Slot)
		.Padding(Padding)
		.FlexGrow(FlexGrow)
		.FlexShrink(FlexShrink)
		.FlexBasis(NegativeToUnset(FlexBasis))
		.MinMain(NegativeToUnset(MinMainSize))
		.AlignSelf(ToSlate(AlignSelf))
		.Order(Order)
		[
			Content == nullptr ? SNullWidget::NullWidget : Content->TakeWidget()
		];
}

void UFlexBoxSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Slot = nullptr;
}

float UFlexBoxSlot::GetFlexGrow() const
{
	return FlexGrow;
}

void UFlexBoxSlot::SetFlexGrow(float InFlexGrow)
{
	FlexGrow = InFlexGrow;
	if (Slot)
	{
		Slot->SetFlexGrow(InFlexGrow);
	}
}

float UFlexBoxSlot::GetFlexShrink() const
{
	return FlexShrink;
}

void UFlexBoxSlot::SetFlexShrink(float InFlexShrink)
{
	FlexShrink = InFlexShrink;
	if (Slot)
	{
		Slot->SetFlexShrink(InFlexShrink);
	}
}

float UFlexBoxSlot::GetFlexBasis() const
{
	return FlexBasis;
}

void UFlexBoxSlot::SetFlexBasis(float InFlexBasis)
{
	FlexBasis = InFlexBasis;
	if (Slot)
	{
		Slot->SetFlexBasis(UFlexBoxSlotPrivate::NegativeToUnset(InFlexBasis));
	}
}

float UFlexBoxSlot::GetMinMainSize() const
{
	return MinMainSize;
}

void UFlexBoxSlot::SetMinMainSize(float InMinMainSize)
{
	MinMainSize = InMinMainSize;
	if (Slot)
	{
		Slot->SetMinMain(UFlexBoxSlotPrivate::NegativeToUnset(InMinMainSize));
	}
}

EArcFlexAlign UFlexBoxSlot::GetAlignSelf() const
{
	return AlignSelf;
}

void UFlexBoxSlot::SetAlignSelf(EArcFlexAlign InAlignSelf)
{
	AlignSelf = InAlignSelf;
	if (Slot)
	{
		Slot->SetAlignSelf(UFlexBoxSlotPrivate::ToSlate(InAlignSelf));
	}
}

int32 UFlexBoxSlot::GetOrder() const
{
	return Order;
}

void UFlexBoxSlot::SetOrder(int32 InOrder)
{
	Order = InOrder;
	if (Slot)
	{
		Slot->SetOrder(InOrder);
	}
}

FMargin UFlexBoxSlot::GetPadding() const
{
	return Padding;
}

void UFlexBoxSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if (Slot)
	{
		Slot->SetPadding(InPadding);
	}
}

void UFlexBoxSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetFlexGrow(FlexGrow);
	SetFlexShrink(FlexShrink);
	SetFlexBasis(FlexBasis);
	SetMinMainSize(MinMainSize);
	SetAlignSelf(AlignSelf);
	SetOrder(Order);
}
