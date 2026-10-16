// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StyledBox/UStyledBox.h"
#include "ArcSlotChrome.generated.h"

/**
 * A shared, themed color-set describing the translucent-glass chrome of a slot
 * tile: a faint white fill plus a slightly stronger white outline.
 *
 * Reused by both UArcIconChip and UArcSlotButtonBase so the chrome constants
 * live in exactly one place instead of drifting across per-widget copies.
 */
USTRUCT(BlueprintType)
struct FArcSlotChromeColors
{
	GENERATED_BODY()

	/** Fill color of the rounded background. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Chrome")
	FLinearColor BackgroundColor = FLinearColor(1.f, 1.f, 1.f, 0.035f);

	/** Color of the outline drawn around the rounded box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Chrome")
	FLinearColor BorderColor = FLinearColor(1.f, 1.f, 1.f, 0.08f);
};

/**
 * Pushes a chrome color-set + border width onto a UStyledBox. This is the ONLY
 * definition of the chrome-application logic; both slot classes call it so the
 * fill/border/width mapping never diverges.
 */
inline void ArcApplyChrome(UStyledBox& Box, const FArcSlotChromeColors& Colors, float BorderWidth)
{
	Box.SetBackgroundColor(Colors.BackgroundColor);
	Box.SetBorderColor(Colors.BorderColor);
	Box.SetBorderWidth(BorderWidth);
}
