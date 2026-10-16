// Copyright Lukasz Baran. All Rights Reserved.

#include "Gradient/UGradientImage.h"

#include "Widgets/Colors/SComplexGradient.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"

// ---------------------------------------------------------------------------
// RebuildWidget / SynchronizeProperties / ReleaseSlateResources
// ---------------------------------------------------------------------------

TSharedRef<SWidget> UGradientImage::RebuildWidget()
{
	MyRoot = SNew(SBox);
	RebuildGradient();
	return MyRoot.ToSharedRef();
}

void UGradientImage::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	RebuildGradient();
}

void UGradientImage::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyGradient.Reset();
	MyRoot.Reset();
}

// ---------------------------------------------------------------------------
// RebuildGradient (private)
// ---------------------------------------------------------------------------

void UGradientImage::RebuildGradient()
{
	if (!MyRoot.IsValid())
	{
		return;
	}

	if (GradientColors.Num() > 1)
	{
		MyGradient = SNew(SComplexGradient)
			.GradientColors(GradientColors)
			.Orientation(Orientation)
			.HasAlphaBackground(bHasAlphaBackground);

		MyRoot->SetContent(MyGradient.ToSharedRef());
	}
	else
	{
		MyRoot->SetContent(SNullWidget::NullWidget);
		MyGradient.Reset();
	}
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

TArray<FLinearColor> UGradientImage::GetGradientColors() const
{
	return GradientColors;
}

EOrientation UGradientImage::GetOrientation() const
{
	return Orientation;
}

bool UGradientImage::GetHasAlphaBackground() const
{
	return bHasAlphaBackground;
}

// ---------------------------------------------------------------------------
// Setters
// ---------------------------------------------------------------------------

void UGradientImage::SetGradientColors(TArray<FLinearColor> InGradientColors)
{
	GradientColors = MoveTemp(InGradientColors);
	RebuildGradient();
}

void UGradientImage::SetOrientation(EOrientation InOrientation)
{
	Orientation = InOrientation;
	RebuildGradient();
}

void UGradientImage::SetHasAlphaBackground(bool bInHasAlphaBackground)
{
	bHasAlphaBackground = bInHasAlphaBackground;
	RebuildGradient();
}
