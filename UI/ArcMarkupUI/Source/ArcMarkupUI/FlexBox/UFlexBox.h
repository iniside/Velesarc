// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PanelWidget.h"
#include "FlexBox/UFlexBoxSlot.h"
#include "UFlexBox.generated.h"

class SFlexBox;
class UFlexBoxSlot;
class UWidget;

/**
 * A CSS flexbox layout container.
 *
 * Wraps SFlexBox with full UMG Blueprint support. Arranges children along a
 * configurable main axis with optional wrapping, gap, per-item flex grow/shrink,
 * and main/cross-axis alignment.
 *
 * * Many Children
 * * Flex layout (row or column, optional wrap)
 */
UCLASS(meta = (ShortTooltip = "A CSS flexbox layout container for flexible child widget arrangement"), MinimalAPI)
class UFlexBox : public UPanelWidget
{
	GENERATED_BODY()

public:
	ARCMARKUPUI_API UFlexBox();

private:
	/** CSS flex-direction: determines the main axis and item order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Flex", meta = (AllowPrivateAccess = "true"))
	EArcFlexDirection Direction = EArcFlexDirection::Row;

	/** CSS flex-wrap: whether items wrap onto multiple lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Flex", meta = (AllowPrivateAccess = "true"))
	EArcFlexWrap Wrap = EArcFlexWrap::NoWrap;

	/** CSS justify-content: distributes free space along the main axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Flex", meta = (AllowPrivateAccess = "true"))
	EArcFlexJustify JustifyContent = EArcFlexJustify::FlexStart;

	/** CSS align-items: cross-axis alignment for all items (overridable per slot via AlignSelf). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Flex", meta = (AllowPrivateAccess = "true"))
	EArcFlexAlign AlignItems = EArcFlexAlign::Stretch;

	/** CSS align-content: cross-axis alignment of the flex lines when there is free space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Flex", meta = (AllowPrivateAccess = "true"))
	EArcFlexAlign AlignContent = EArcFlexAlign::Stretch;

	/** X = column-gap (between items on a line), Y = row-gap (between lines). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Flex", meta = (AllowPrivateAccess = "true"))
	FVector2D Gap = FVector2D::ZeroVector;

public:
	ARCMARKUPUI_API EArcFlexDirection GetDirection() const;
	ARCMARKUPUI_API void SetDirection(EArcFlexDirection InDirection);

	ARCMARKUPUI_API EArcFlexWrap GetWrap() const;
	ARCMARKUPUI_API void SetWrap(EArcFlexWrap InWrap);

	ARCMARKUPUI_API EArcFlexJustify GetJustifyContent() const;
	ARCMARKUPUI_API void SetJustifyContent(EArcFlexJustify InJustifyContent);

	ARCMARKUPUI_API EArcFlexAlign GetAlignItems() const;
	ARCMARKUPUI_API void SetAlignItems(EArcFlexAlign InAlignItems);

	ARCMARKUPUI_API EArcFlexAlign GetAlignContent() const;
	ARCMARKUPUI_API void SetAlignContent(EArcFlexAlign InAlignContent);

	ARCMARKUPUI_API FVector2D GetGap() const;
	ARCMARKUPUI_API void SetGap(FVector2D InGap);

	/** Adds a new child widget to the flex container. */
	UFUNCTION(BlueprintCallable, Category = "Panel")
	ARCMARKUPUI_API UFlexBoxSlot* AddChildToFlexBox(UWidget* Content);

protected:
	//~ UPanelWidget interface
	ARCMARKUPUI_API virtual UClass* GetSlotClass() const override;
	ARCMARKUPUI_API virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	ARCMARKUPUI_API virtual void OnSlotRemoved(UPanelSlot* InSlot) override;
	ARCMARKUPUI_API virtual TSharedRef<SWidget> RebuildWidget() override;
	ARCMARKUPUI_API virtual void SynchronizeProperties() override;
	//~ End UPanelWidget interface

	ARCMARKUPUI_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	TSharedPtr<SFlexBox> MyFlexBox;
};
