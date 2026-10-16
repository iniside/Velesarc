// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "Layout/BasicLayoutWidgetSlot.h"
#include "Layout/Children.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SPanel.h"
#include "Widgets/SWidget.h"

class FArrangedChildren;

/** CSS flex-direction. Determines the main axis and item order. */
enum class EFlexDirection : uint8
{
	Row,
	RowReverse,
	Column,
	ColumnReverse
};

/** CSS flex-wrap. Determines whether items wrap onto multiple lines and the cross-axis line order. */
enum class EFlexWrap : uint8
{
	NoWrap,
	Wrap,
	WrapReverse
};

/** CSS justify-content. Distributes free space along the main axis. */
enum class EFlexJustify : uint8
{
	FlexStart,
	FlexEnd,
	Center,
	SpaceBetween,
	SpaceAround,
	SpaceEvenly
};

/** CSS align-items / align-self / align-content. Positions items (or lines) along the cross axis. NOTE: no Baseline by design. */
enum class EFlexAlign : uint8
{
	Auto,
	FlexStart,
	FlexEnd,
	Center,
	Stretch
};

template <>
struct TWidgetTypeTraits<class SFlexBox>
{
	static constexpr bool SupportsInvalidation() { return true; }
};

/**
 * A CSS-flexbox layout container.
 *
 * Arranges its children along a main axis (chosen by Direction) with optional
 * line-wrapping, grow/shrink free-space distribution, gaps, per-item ordering,
 * and main/cross-axis alignment. Mirrors the CSS flexbox layout algorithm.
 */
class SFlexBox : public SPanel
{
	SLATE_DECLARE_WIDGET_API(SFlexBox, SPanel, ARCMARKUPUI_API)

public:
	/** A slot for SFlexBox. Inherits Padding + HAlign/VAlign from TBasicLayoutWidgetSlot and adds flex item properties. */
	class FSlot : public TBasicLayoutWidgetSlot<FSlot>
	{
	public:
		FSlot()
			: TBasicLayoutWidgetSlot<FSlot>(HAlign_Fill, VAlign_Fill)
			, FlexGrow(0.f)
			, FlexShrink(1.f)
			, FlexBasis()
			, MinMain()
			, AlignSelf(EFlexAlign::Auto)
			, Order(0)
		{
		}

		SLATE_SLOT_BEGIN_ARGS(FSlot, TBasicLayoutWidgetSlot<FSlot>)
			SLATE_ARGUMENT(TOptional<float>, FlexGrow)
			SLATE_ARGUMENT(TOptional<float>, FlexShrink)
			SLATE_ARGUMENT(TOptional<float>, FlexBasis)
			SLATE_ARGUMENT(TOptional<float>, MinMain)
			SLATE_ARGUMENT(TOptional<EFlexAlign>, AlignSelf)
			SLATE_ARGUMENT(TOptional<int32>, Order)
		SLATE_SLOT_END_ARGS()

		void Construct(const FChildren& SlotOwner, FSlotArguments&& InArgs)
		{
			TBasicLayoutWidgetSlot<FSlot>::Construct(SlotOwner, MoveTemp(InArgs));
			FlexGrow = InArgs._FlexGrow.Get(FlexGrow);
			FlexShrink = InArgs._FlexShrink.Get(FlexShrink);
			if (InArgs._FlexBasis.IsSet())
			{
				FlexBasis = InArgs._FlexBasis;
			}
			if (InArgs._MinMain.IsSet())
			{
				MinMain = InArgs._MinMain;
			}
			AlignSelf = InArgs._AlignSelf.Get(AlignSelf);
			Order = InArgs._Order.Get(Order);
		}

		static void RegisterAttributes(FSlateWidgetSlotAttributeInitializer& AttributeInitializer)
		{
			TBasicLayoutWidgetSlot<FSlot>::RegisterAttributes(AttributeInitializer);
		}

		/** flex-grow: proportion of positive free space this item absorbs. 0 = does not grow. */
		void SetFlexGrow(float InFlexGrow)
		{
			if (FlexGrow != InFlexGrow)
			{
				FlexGrow = InFlexGrow;
				FSlotBase::Invalidate(EInvalidateWidgetReason::Layout);
			}
		}

		float GetFlexGrow() const
		{
			return FlexGrow;
		}

		/** flex-shrink: proportion (scaled by basis) of negative free space this item gives up. 0 = does not shrink. */
		void SetFlexShrink(float InFlexShrink)
		{
			if (FlexShrink != InFlexShrink)
			{
				FlexShrink = InFlexShrink;
				FSlotBase::Invalidate(EInvalidateWidgetReason::Layout);
			}
		}

		float GetFlexShrink() const
		{
			return FlexShrink;
		}

		/** flex-basis: starting main-axis size. Unset = use the child's desired main-axis size as basis. */
		void SetFlexBasis(TOptional<float> InFlexBasis)
		{
			if (FlexBasis != InFlexBasis)
			{
				FlexBasis = InFlexBasis;
				FSlotBase::Invalidate(EInvalidateWidgetReason::Layout);
			}
		}

		TOptional<float> GetFlexBasis() const
		{
			return FlexBasis;
		}

		/**
		 * min main-axis size (CSS min-width / min-height). Unset = auto: the item never shrinks below
		 * its min-content size (approximated by the child's desired main-axis size). Set explicitly to
		 * 0 to allow full collapse (CSS min-*:0).
		 */
		void SetMinMain(TOptional<float> InMinMain)
		{
			if (MinMain != InMinMain)
			{
				MinMain = InMinMain;
				FSlotBase::Invalidate(EInvalidateWidgetReason::Layout);
			}
		}

		TOptional<float> GetMinMain() const
		{
			return MinMain;
		}

		/** align-self: cross-axis alignment override for this item. Auto = inherit the container's AlignItems. */
		void SetAlignSelf(EFlexAlign InAlignSelf)
		{
			if (AlignSelf != InAlignSelf)
			{
				AlignSelf = InAlignSelf;
				FSlotBase::Invalidate(EInvalidateWidgetReason::Layout);
			}
		}

		EFlexAlign GetAlignSelf() const
		{
			return AlignSelf;
		}

		/** order: items are laid out by ascending order; ties keep source order (stable). */
		void SetOrder(int32 InOrder)
		{
			if (Order != InOrder)
			{
				Order = InOrder;
				FSlotBase::Invalidate(EInvalidateWidgetReason::Layout);
			}
		}

		int32 GetOrder() const
		{
			return Order;
		}

	private:
		float FlexGrow;
		float FlexShrink;
		TOptional<float> FlexBasis;
		TOptional<float> MinMain;
		EFlexAlign AlignSelf;
		int32 Order;
	};

	SLATE_BEGIN_ARGS(SFlexBox)
		: _Direction(EFlexDirection::Row)
		, _Wrap(EFlexWrap::NoWrap)
		, _JustifyContent(EFlexJustify::FlexStart)
		, _AlignItems(EFlexAlign::Stretch)
		, _AlignContent(EFlexAlign::Stretch)
		, _Gap(FVector2f::ZeroVector)
	{
		_Visibility = EVisibility::SelfHitTestInvisible;
	}
		SLATE_ARGUMENT(EFlexDirection, Direction)
		SLATE_ARGUMENT(EFlexWrap, Wrap)
		SLATE_ARGUMENT(EFlexJustify, JustifyContent)
		SLATE_ARGUMENT(EFlexAlign, AlignItems)
		SLATE_ARGUMENT(EFlexAlign, AlignContent)

		/** X = column-gap (between items on a line), Y = row-gap (between lines). */
		SLATE_ARGUMENT(FVector2f, Gap)

		SLATE_SLOT_ARGUMENT(FSlot, Slots)
	SLATE_END_ARGS()

	ARCMARKUPUI_API SFlexBox();
	ARCMARKUPUI_API virtual ~SFlexBox();

	ARCMARKUPUI_API void Construct(const FArguments& InArgs);

	//~ Begin SWidget overrides.
	ARCMARKUPUI_API virtual void OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const override;
	virtual FChildren* GetChildren() override { return &Children; }

protected:
	ARCMARKUPUI_API virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	//~ End SWidget overrides.

public:
	/** Build a detached slot for use with the declarative syntax. */
	static FSlot::FSlotArguments Slot()
	{
		return FSlot::FSlotArguments(MakeUnique<FSlot>());
	}

	using FScopedWidgetSlotArguments = TPanelChildren<FSlot>::FScopedWidgetSlotArguments;

	/** Add a slot at the end. */
	ARCMARKUPUI_API FScopedWidgetSlotArguments AddSlot();

	/** Insert a slot at the given index (INDEX_NONE appends). */
	ARCMARKUPUI_API FScopedWidgetSlotArguments InsertSlot(int32 Index = INDEX_NONE);

	/** Remove the slot containing the given widget. @return removed index, or INDEX_NONE. */
	ARCMARKUPUI_API int32 RemoveSlot(const TSharedRef<SWidget>& SlotWidget);

	/** Remove all slots. */
	ARCMARKUPUI_API void ClearChildren();

	int32 NumSlots() const { return Children.Num(); }
	bool IsValidSlotIndex(int32 Index) const { return Children.IsValidIndex(Index); }

	ARCMARKUPUI_API FSlot& GetSlot(int32 SlotIndex);
	ARCMARKUPUI_API const FSlot& GetSlot(int32 SlotIndex) const;

	//~ Container property setters. All trigger a layout invalidation.
	ARCMARKUPUI_API void SetDirection(EFlexDirection InDirection);
	ARCMARKUPUI_API void SetWrap(EFlexWrap InWrap);
	ARCMARKUPUI_API void SetJustifyContent(EFlexJustify InJustifyContent);
	ARCMARKUPUI_API void SetAlignItems(EFlexAlign InAlignItems);
	ARCMARKUPUI_API void SetAlignContent(EFlexAlign InAlignContent);
	ARCMARKUPUI_API void SetGap(FVector2f InGap);

	EFlexDirection GetDirection() const { return Direction; }
	EFlexWrap GetWrap() const { return Wrap; }
	EFlexJustify GetJustifyContent() const { return JustifyContent; }
	EFlexAlign GetAlignItems() const { return AlignItems; }
	EFlexAlign GetAlignContent() const { return AlignContent; }
	FVector2f GetGap() const { return Gap; }

private:
	/** This panel's children. */
	TPanelChildren<FSlot> Children;

	EFlexDirection Direction;
	EFlexWrap Wrap;
	EFlexJustify JustifyContent;
	EFlexAlign AlignItems;
	EFlexAlign AlignContent;

	/** X = column-gap (between items on a line), Y = row-gap (between lines). */
	FVector2f Gap;
};
