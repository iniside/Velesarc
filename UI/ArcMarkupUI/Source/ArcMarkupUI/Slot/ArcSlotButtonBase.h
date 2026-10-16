// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Slot/ArcSlotChrome.h"
#include "ArcSlotButtonBase.generated.h"

class UButton;
class UStyledBox;
class UArcSvgImage;
class UTextBlock;
class UArcSvgAsset;
class UArcSlotButtonBase;

/** Fires when the slot is clicked, passing the clicked slot. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcSlotClicked, UArcSlotButtonBase*, Slot);

/**
 * An interactive click tile: a null-styled UButton wrapping a rounded
 * translucent-glass StyledBox frame, an optional SVG icon and label. Absorbs the
 * copy-pasted hover/press/selected tinting + chrome logic from the Family-A
 * click tiles (NavButton, QuickSlot, EquipmentSlot).
 *
 * The frame retints per state (Normal / Hover / Selected); hover is suppressed
 * while selected. Any skin reparented to this class MUST provide exactly-named
 * child widgets:
 *   - SlotButton (required BindWidget, UButton)
 *   - SlotFrame  (required BindWidget, UStyledBox)
 * The icon SlotIcon (UArcSvgImage) and label SlotLabel (UTextBlock) are optional.
 */
UCLASS(MinimalAPI, meta = (ShortTooltip = "An interactive themed click tile"))
class UArcSlotButtonBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the selected state; retints and (if changed) is reflected by IsSelected. */
	UFUNCTION(BlueprintCallable, Category = "Slot Button")
	ARCMARKUPUI_API void SetSelected(bool bInSelected);

	/** Whether the slot is currently selected. */
	UFUNCTION(BlueprintCallable, Category = "Slot Button")
	ARCMARKUPUI_API bool IsSelected() const { return bSelected; }

	/** This slot's index (e.g. nav tab / equipment slot ordinal). */
	UFUNCTION(BlueprintCallable, Category = "Slot Button")
	ARCMARKUPUI_API int32 GetSlotIndex() const { return SlotIndex; }

	/** Fires when the slot is clicked, passing this slot. */
	UPROPERTY(BlueprintAssignable, Category = "Slot Button|Event")
	FArcSlotClicked OnSlotClicked;

protected:
	//~ Begin UUserWidget interface
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	//~ End UUserWidget interface

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	/** Applies the state-appropriate chrome + corner radii to the frame and tints the icon. */
	void RefreshVisualState();

	/** Interactive button; required in any skin. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SlotButton;

	/** Rounded translucent-glass frame; required in any skin. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UStyledBox> SlotFrame;

	/** Optional centered vector icon. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UArcSvgImage> SlotIcon;

	/** Optional label. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SlotLabel;

	/** Chrome color-set for the idle state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button|Chrome", meta = (AllowPrivateAccess = "true"))
	FArcSlotChromeColors Normal;

	/** Chrome color-set while hovered (suppressed when selected). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button|Chrome", meta = (AllowPrivateAccess = "true"))
	FArcSlotChromeColors Hover;

	/** Chrome color-set while selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button|Chrome", meta = (AllowPrivateAccess = "true"))
	FArcSlotChromeColors Selected;

	/** Outline thickness in Slate Units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button", meta = (AllowPrivateAccess = "true"))
	float BorderWidth = 1.f;

	/** Per-corner radii (TL, TR, BR, BL) in Slate Units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button", meta = (AllowPrivateAccess = "true"))
	FVector4 CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);

	/** Icon tint when not selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button", meta = (AllowPrivateAccess = "true"))
	FLinearColor IconColor = FLinearColor(1.f, 1.f, 1.f, 0.82f);

	/** Icon tint when selected. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button", meta = (AllowPrivateAccess = "true"))
	FLinearColor IconSelectedColor = FLinearColor(0.65f, 0.43f, 0.15f, 1.f);

	/** Label text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button", meta = (AllowPrivateAccess = "true"))
	FText LabelText;

	/** SVG asset drawn in the tile. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArcSvgAsset> IconSvg;

	/** Draw size of the icon in Slate Units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button", meta = (AllowPrivateAccess = "true"))
	FVector2D IconSize = FVector2D(22.f, 22.f);

	/** This slot's index; broadcast consumers read it via GetSlotIndex. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Button", meta = (AllowPrivateAccess = "true"))
	int32 SlotIndex = 0;

	/** Runtime selected state. */
	bool bSelected = false;

	/** Runtime hover state. */
	bool bHovered = false;
};
