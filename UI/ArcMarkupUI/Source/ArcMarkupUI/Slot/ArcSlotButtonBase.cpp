// Copyright Lukasz Baran. All Rights Reserved.

#include "Slot/ArcSlotButtonBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "StyledBox/UStyledBox.h"
#include "Svg/ArcSvgImage.h"

void UArcSlotButtonBase::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (SlotButton)
	{
		FButtonStyle Style = SlotButton->GetStyle();
		Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
		Style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
		SlotButton->SetStyle(Style);
	}

	if (SlotLabel)
	{
		SlotLabel->SetText(LabelText);
	}

	if (SlotIcon)
	{
		SlotIcon->SetSvgAsset(IconSvg);
		SlotIcon->SetOverrideDrawSize(true);
		SlotIcon->SetIconSize(IconSize);
	}

	RefreshVisualState();
}

void UArcSlotButtonBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (SlotButton)
	{
		SlotButton->OnClicked.AddUniqueDynamic(this, &UArcSlotButtonBase::HandleClicked);
		SlotButton->OnHovered.AddUniqueDynamic(this, &UArcSlotButtonBase::HandleHovered);
		SlotButton->OnUnhovered.AddUniqueDynamic(this, &UArcSlotButtonBase::HandleUnhovered);
	}
}

void UArcSlotButtonBase::HandleClicked()
{
	OnSlotClicked.Broadcast(this);
}

void UArcSlotButtonBase::HandleHovered()
{
	bHovered = true;
	RefreshVisualState();
}

void UArcSlotButtonBase::HandleUnhovered()
{
	bHovered = false;
	RefreshVisualState();
}

void UArcSlotButtonBase::SetSelected(bool bInSelected)
{
	if (bSelected == bInSelected)
	{
		return;
	}

	bSelected = bInSelected;
	RefreshVisualState();
}

void UArcSlotButtonBase::RefreshVisualState()
{
	const FArcSlotChromeColors& Chrome = bSelected ? Selected : (bHovered ? Hover : Normal);

	if (SlotFrame)
	{
		ArcApplyChrome(*SlotFrame, Chrome, BorderWidth);
		SlotFrame->SetCornerRadii(CornerRadii);
	}

	if (SlotIcon)
	{
		SlotIcon->SetColorAndOpacity(bSelected ? IconSelectedColor : IconColor);
	}
}
