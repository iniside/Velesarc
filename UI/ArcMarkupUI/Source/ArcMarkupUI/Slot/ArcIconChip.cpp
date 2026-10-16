// Copyright Lukasz Baran. All Rights Reserved.

#include "Slot/ArcIconChip.h"

#include "StyledBox/UStyledBox.h"
#include "Svg/ArcSvgImage.h"

void UArcIconChip::NativePreConstruct()
{
	Super::NativePreConstruct();

	RefreshVisualState();
}

void UArcIconChip::SetIcon(UArcSvgAsset* InSvg)
{
	IconSvg = InSvg;
	RefreshVisualState();
}

void UArcIconChip::SetIconColor(FLinearColor InColor)
{
	IconColor = InColor;
	RefreshVisualState();
}

void UArcIconChip::SetChrome(const FArcSlotChromeColors& InChrome)
{
	ChromeColors = InChrome;
	RefreshVisualState();
}

void UArcIconChip::RefreshVisualState()
{
	if (Frame)
	{
		ArcApplyChrome(*Frame, ChromeColors, BorderWidth);
		Frame->SetCornerRadii(CornerRadii);
	}

	if (Icon)
	{
		Icon->SetSvgAsset(IconSvg);
		Icon->SetOverrideDrawSize(true);
		Icon->SetIconSize(IconSize);
		Icon->SetColorAndOpacity(IconColor);
	}
}
