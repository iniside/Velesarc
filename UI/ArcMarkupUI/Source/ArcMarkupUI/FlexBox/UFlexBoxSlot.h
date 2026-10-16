// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Components/PanelSlot.h"
#include "FlexBox/SFlexBox.h"
#include "UFlexBoxSlot.generated.h"

class SFlexBox;
class UWidget;

/** CSS flex-direction mirror. */
UENUM(BlueprintType)
enum class EArcFlexDirection : uint8
{
	Row,
	RowReverse,
	Column,
	ColumnReverse
};

/** CSS flex-wrap mirror. */
UENUM(BlueprintType)
enum class EArcFlexWrap : uint8
{
	NoWrap,
	Wrap,
	WrapReverse
};

/** CSS justify-content mirror. */
UENUM(BlueprintType)
enum class EArcFlexJustify : uint8
{
	FlexStart,
	FlexEnd,
	Center,
	SpaceBetween,
	SpaceAround,
	SpaceEvenly
};

/** CSS align-items / align-self / align-content mirror. No Baseline by design. */
UENUM(BlueprintType)
enum class EArcFlexAlign : uint8
{
	Auto,
	FlexStart,
	FlexEnd,
	Center,
	Stretch
};

/** The slot for UFlexBox — carries per-item flex properties. */
UCLASS(MinimalAPI)
class UFlexBoxSlot : public UPanelSlot
{
	GENERATED_BODY()

private:
	/** flex-grow: proportion of positive free space this item absorbs. 0 = does not grow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout|Flex Box Slot", meta = (AllowPrivateAccess = "true"))
	float FlexGrow = 0.f;

	/** flex-shrink: proportion of negative free space this item gives up. 0 = does not shrink. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout|Flex Box Slot", meta = (AllowPrivateAccess = "true"))
	float FlexShrink = 1.f;

	/**
	 * flex-basis: starting main-axis size.
	 * Values < 0 are treated as auto (unset — the child's desired main-axis size is used).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout|Flex Box Slot", meta = (AllowPrivateAccess = "true"))
	float FlexBasis = -1.f;

	/**
	 * min main-axis size (CSS min-width / min-height for this item).
	 * Values < 0 are auto: the item never shrinks below its min-content (desired) main-axis size,
	 * so its content is not squeezed past its edges. Set to 0 to allow full collapse (CSS min-*:0).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout|Flex Box Slot", meta = (AllowPrivateAccess = "true"))
	float MinMainSize = -1.f;

	/** align-self: cross-axis alignment override for this item. Auto = inherit the container's AlignItems. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout|Flex Box Slot", meta = (AllowPrivateAccess = "true"))
	EArcFlexAlign AlignSelf = EArcFlexAlign::Auto;

	/** order: items are laid out by ascending order; ties keep source order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout|Flex Box Slot", meta = (AllowPrivateAccess = "true"))
	int32 Order = 0;

	/** Padding around the slot content. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Layout|Flex Box Slot", meta = (AllowPrivateAccess = "true"))
	FMargin Padding;

private:
	/** Raw pointer to the live Slate slot. Null until BuildSlot is called. */
	SFlexBox::FSlot* Slot = nullptr;

public:
	ARCMARKUPUI_API float GetFlexGrow() const;
	ARCMARKUPUI_API void SetFlexGrow(float InFlexGrow);

	ARCMARKUPUI_API float GetFlexShrink() const;
	ARCMARKUPUI_API void SetFlexShrink(float InFlexShrink);

	ARCMARKUPUI_API float GetFlexBasis() const;
	ARCMARKUPUI_API void SetFlexBasis(float InFlexBasis);

	ARCMARKUPUI_API float GetMinMainSize() const;
	ARCMARKUPUI_API void SetMinMainSize(float InMinMainSize);

	ARCMARKUPUI_API EArcFlexAlign GetAlignSelf() const;
	ARCMARKUPUI_API void SetAlignSelf(EArcFlexAlign InAlignSelf);

	ARCMARKUPUI_API int32 GetOrder() const;
	ARCMARKUPUI_API void SetOrder(int32 InOrder);

	ARCMARKUPUI_API FMargin GetPadding() const;
	ARCMARKUPUI_API void SetPadding(FMargin InPadding);

	/** Builds the underlying FSlot in the given SFlexBox. Called by UFlexBox::RebuildWidget / OnSlotAdded. */
	ARCMARKUPUI_API void BuildSlot(TSharedRef<SFlexBox> InFlexBox);

	//~ UPanelSlot interface
	ARCMARKUPUI_API virtual void SynchronizeProperties() override;
	//~ End UPanelSlot interface

	ARCMARKUPUI_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
};
