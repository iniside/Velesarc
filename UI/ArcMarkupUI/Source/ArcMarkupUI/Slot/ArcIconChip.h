// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Slot/ArcSlotChrome.h"
#include "ArcIconChip.generated.h"

class UStyledBox;
class UArcSvgImage;
class UArcSvgAsset;

/**
 * A non-interactive themed icon tile: a rounded translucent-glass StyledBox frame
 * with a centered SVG icon. The canonical primitive that consolidates the
 * copy-pasted "SizeBox -> StyledBox -> ArcSvgImage" chip shape used across the UI.
 *
 * Any skin reparented to this class MUST provide an exactly-named child widget:
 *   - Frame  (required BindWidget, UStyledBox)
 * The icon Icon (UArcSvgImage) is optional (BindWidgetOptional).
 */
UCLASS(MinimalAPI, meta = (ShortTooltip = "A non-interactive themed icon tile"))
class UArcIconChip : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Sets the icon SVG and refreshes visuals. */
	UFUNCTION(BlueprintCallable, Category = "Icon Chip")
	ARCMARKUPUI_API void SetIcon(UArcSvgAsset* InSvg);

	/** Sets the icon tint and refreshes visuals. */
	UFUNCTION(BlueprintCallable, Category = "Icon Chip")
	ARCMARKUPUI_API void SetIconColor(FLinearColor InColor);

	/** Sets the chrome color-set and refreshes visuals. */
	UFUNCTION(BlueprintCallable, Category = "Icon Chip")
	ARCMARKUPUI_API void SetChrome(const FArcSlotChromeColors& InChrome);

protected:
	//~ Begin UUserWidget interface
	virtual void NativePreConstruct() override;
	//~ End UUserWidget interface

	/** Pushes chrome + corner radii to Frame and svg/size/color to Icon. */
	void RefreshVisualState();

	/** Rounded translucent-glass frame; required in any skin. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UStyledBox> Frame;

	/** Optional centered vector icon. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UArcSvgImage> Icon;

	/** Fill + outline color-set for the frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Chip", meta = (AllowPrivateAccess = "true"))
	FArcSlotChromeColors ChromeColors;

	/** Outline thickness in Slate Units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Chip", meta = (AllowPrivateAccess = "true"))
	float BorderWidth = 1.f;

	/** Per-corner radii (TL, TR, BR, BL) in Slate Units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Chip", meta = (AllowPrivateAccess = "true"))
	FVector4 CornerRadii = FVector4(10.f, 10.f, 10.f, 10.f);

	/** SVG asset drawn in the chip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Chip", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UArcSvgAsset> IconSvg;

	/** Draw size of the icon in Slate Units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Chip", meta = (AllowPrivateAccess = "true"))
	FVector2D IconSize = FVector2D(22.f, 22.f);

	/** Icon tint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Chip", meta = (AllowPrivateAccess = "true"))
	FLinearColor IconColor = FLinearColor(1.f, 1.f, 1.f, 0.82f);
};
