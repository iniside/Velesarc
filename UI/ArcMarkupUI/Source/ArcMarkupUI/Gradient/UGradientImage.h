// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Types/SlateEnums.h"
#include "UGradientImage.generated.h"

class SBox;
class SComplexGradient;

/**
 * A UMG widget that renders a multi-stop linear gradient via Slate's SComplexGradient.
 *
 * SComplexGradient has no runtime property setters (GradientColors is a private
 * TSlateAttribute). To support live property updates from Blueprint/editor, the root
 * widget is an SBox holder; on any change the gradient is fully rebuilt and pushed into
 * the holder via SetContent().
 *
 * Linear only. Radial/conic gradients are out of scope — use a UMaterial brush path
 * instead.
 *
 * * No Children
 */
UCLASS(MinimalAPI, meta = (ShortTooltip = "Renders a multi-stop linear gradient"))
class UGradientImage : public UWidget
{
	GENERATED_BODY()

public:
	//~ Begin UWidget interface
	ARCMARKUPUI_API virtual void SynchronizeProperties() override;
	ARCMARKUPUI_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	//~ End UWidget interface

	// --- Getters ---

	ARCMARKUPUI_API TArray<FLinearColor> GetGradientColors() const;
	ARCMARKUPUI_API EOrientation GetOrientation() const;
	ARCMARKUPUI_API bool GetHasAlphaBackground() const;

	// --- Setters ---

	UFUNCTION(BlueprintCallable, Category = "Gradient")
	ARCMARKUPUI_API void SetGradientColors(TArray<FLinearColor> InGradientColors);

	UFUNCTION(BlueprintCallable, Category = "Gradient")
	ARCMARKUPUI_API void SetOrientation(EOrientation InOrientation);

	UFUNCTION(BlueprintCallable, Category = "Gradient")
	ARCMARKUPUI_API void SetHasAlphaBackground(bool bInHasAlphaBackground);

protected:
	//~ Begin UWidget interface
	ARCMARKUPUI_API virtual TSharedRef<SWidget> RebuildWidget() override;
	//~ End UWidget interface

private:
	/** The color stops of the gradient. Requires at least 2 colors to render. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Gradient", meta = (AllowPrivateAccess = "true"))
	TArray<FLinearColor> GradientColors;

	/** Whether the gradient is laid out horizontally or vertically. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Gradient", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<EOrientation> Orientation = Orient_Vertical;

	/** When true, draws a checker-pattern background to visualise the alpha channel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter = "SetHasAlphaBackground", Getter = "GetHasAlphaBackground", Category = "Gradient", meta = (AllowPrivateAccess = "true"))
	bool bHasAlphaBackground = false;

	/** Rebuilds MyGradient and places it inside MyRoot, or clears the slot when fewer than 2 colors are supplied. */
	void RebuildGradient();

	TSharedPtr<SBox> MyRoot;
	TSharedPtr<SComplexGradient> MyGradient;
};
