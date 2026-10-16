// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/Border.h"
#include "UStyledBox.generated.h"

/**
 * A UBorder subclass that renders a solid rounded-rectangle background with a
 * per-corner border radius and an optional outline.
 *
 * Exposes BackgroundColor, CornerRadii (TL, TR, BR, BL), BorderColor, and
 * BorderWidth as Blueprint-accessible properties.  All four map onto a single
 * RoundedBox FSlateBrush that is pushed to the inherited SBorder via SetBrush().
 *
 * * Single Child
 * * Rounded background + outline
 */
UCLASS(MinimalAPI, meta = (ShortTooltip = "A border with rounded corners and an optional outline"))
class UStyledBox : public UBorder
{
	GENERATED_BODY()

public:
	//~ Begin UWidget interface
	ARCMARKUPUI_API virtual void SynchronizeProperties() override;
	//~ End UWidget interface

	// --- Getters ---

	ARCMARKUPUI_API FLinearColor GetBackgroundColor() const;
	ARCMARKUPUI_API FVector4 GetCornerRadii() const;
	ARCMARKUPUI_API FLinearColor GetBorderColor() const;
	ARCMARKUPUI_API float GetBorderWidth() const;

	// --- Setters ---

	UFUNCTION(BlueprintCallable, Category = "Styled Box")
	ARCMARKUPUI_API void SetBackgroundColor(FLinearColor InBackgroundColor);

	UFUNCTION(BlueprintCallable, Category = "Styled Box")
	ARCMARKUPUI_API void SetCornerRadii(FVector4 InCornerRadii);

	UFUNCTION(BlueprintCallable, Category = "Styled Box")
	ARCMARKUPUI_API void SetBorderColor(FLinearColor InBorderColor);

	UFUNCTION(BlueprintCallable, Category = "Styled Box")
	ARCMARKUPUI_API void SetBorderWidth(float InBorderWidth);

private:
	/** Fill color of the rounded background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Styled Box", meta = (AllowPrivateAccess = "true"))
	FLinearColor BackgroundColor = FLinearColor::White;

	/** Per-corner radii in Slate Units. X = Top Left, Y = Top Right, Z = Bottom Right, W = Bottom Left. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Styled Box", meta = (AllowPrivateAccess = "true"))
	FVector4 CornerRadii = FVector4(0.f, 0.f, 0.f, 0.f);

	/** Color of the outline drawn around the rounded box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Styled Box", meta = (AllowPrivateAccess = "true"))
	FLinearColor BorderColor = FLinearColor::Transparent;

	/** Thickness of the outline in Slate Units. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Getter, Category = "Styled Box", meta = (AllowPrivateAccess = "true"))
	float BorderWidth = 0.f;

	/** Rebuilds the RoundedBox FSlateBrush from the current property values and calls SetBrush(). */
	void RebuildStyledBrush();
};
