// Copyright Lukasz Baran. All Rights Reserved.

#include "StyledBox/UStyledBox.h"
#include "Styling/SlateBrush.h"

namespace ArcStyledBox
{
	static FSlateBrush BuildRoundedBrush(
		const FLinearColor& InBackgroundColor,
		const FVector4& InCornerRadii,
		const FLinearColor& InBorderColor,
		float InBorderWidth)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(InBackgroundColor);
		Brush.OutlineSettings = FSlateBrushOutlineSettings(
			InCornerRadii,
			FSlateColor(InBorderColor),
			InBorderWidth);
		return Brush;
	}
} // namespace ArcStyledBox

void UStyledBox::RebuildStyledBrush()
{
	FSlateBrush Brush = ArcStyledBox::BuildRoundedBrush(BackgroundColor, CornerRadii, BorderColor, BorderWidth);
	SetBrush(Brush);
}

void UStyledBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	RebuildStyledBrush();
}

// --- Getters ---

FLinearColor UStyledBox::GetBackgroundColor() const
{
	return BackgroundColor;
}

FVector4 UStyledBox::GetCornerRadii() const
{
	return CornerRadii;
}

FLinearColor UStyledBox::GetBorderColor() const
{
	return BorderColor;
}

float UStyledBox::GetBorderWidth() const
{
	return BorderWidth;
}

// --- Setters ---

void UStyledBox::SetBackgroundColor(FLinearColor InBackgroundColor)
{
	BackgroundColor = InBackgroundColor;
	RebuildStyledBrush();
}

void UStyledBox::SetCornerRadii(FVector4 InCornerRadii)
{
	CornerRadii = InCornerRadii;
	RebuildStyledBrush();
}

void UStyledBox::SetBorderColor(FLinearColor InBorderColor)
{
	BorderColor = InBorderColor;
	RebuildStyledBrush();
}

void UStyledBox::SetBorderWidth(float InBorderWidth)
{
	BorderWidth = InBorderWidth;
	RebuildStyledBrush();
}
