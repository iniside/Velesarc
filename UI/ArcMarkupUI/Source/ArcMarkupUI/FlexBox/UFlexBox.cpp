// Copyright Lukasz Baran. All Rights Reserved.

#include "FlexBox/UFlexBox.h"
#include "FlexBox/UFlexBoxSlot.h"
#include "FlexBox/SFlexBox.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UFlexBox)

namespace UFlexBoxPrivate
{
	EFlexDirection ToSlate(EArcFlexDirection In)
	{
		return static_cast<EFlexDirection>(In);
	}

	EFlexWrap ToSlate(EArcFlexWrap In)
	{
		return static_cast<EFlexWrap>(In);
	}

	EFlexJustify ToSlate(EArcFlexJustify In)
	{
		return static_cast<EFlexJustify>(In);
	}

	EFlexAlign ToSlate(EArcFlexAlign In)
	{
		return static_cast<EFlexAlign>(In);
	}
}

/////////////////////////////////////////////////////
// UFlexBox

UFlexBox::UFlexBox()
{
	bCanHaveMultipleChildren = true;
	bIsVariable = false;
	SetVisibilityInternal(ESlateVisibility::SelfHitTestInvisible);
}

void UFlexBox::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	MyFlexBox.Reset();
}

UClass* UFlexBox::GetSlotClass() const
{
	return UFlexBoxSlot::StaticClass();
}

void UFlexBox::OnSlotAdded(UPanelSlot* InSlot)
{
	if (MyFlexBox.IsValid())
	{
		CastChecked<UFlexBoxSlot>(InSlot)->BuildSlot(MyFlexBox.ToSharedRef());
	}
}

void UFlexBox::OnSlotRemoved(UPanelSlot* InSlot)
{
	if (MyFlexBox.IsValid() && InSlot->Content)
	{
		TSharedPtr<SWidget> Widget = InSlot->Content->GetCachedWidget();
		if (Widget.IsValid())
		{
			MyFlexBox->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UFlexBox::RebuildWidget()
{
	using namespace UFlexBoxPrivate;

	MyFlexBox = SNew(SFlexBox)
		.Direction(ToSlate(Direction))
		.Wrap(ToSlate(Wrap))
		.JustifyContent(ToSlate(JustifyContent))
		.AlignItems(ToSlate(AlignItems))
		.AlignContent(ToSlate(AlignContent))
		.Gap(FVector2f((float)Gap.X, (float)Gap.Y));

	for (UPanelSlot* PanelSlot : Slots)
	{
		if (UFlexBoxSlot* TypedSlot = Cast<UFlexBoxSlot>(PanelSlot))
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyFlexBox.ToSharedRef());
		}
	}

	return MyFlexBox.ToSharedRef();
}

void UFlexBox::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if (!MyFlexBox.IsValid())
	{
		return;
	}

	using namespace UFlexBoxPrivate;

	MyFlexBox->SetDirection(ToSlate(Direction));
	MyFlexBox->SetWrap(ToSlate(Wrap));
	MyFlexBox->SetJustifyContent(ToSlate(JustifyContent));
	MyFlexBox->SetAlignItems(ToSlate(AlignItems));
	MyFlexBox->SetAlignContent(ToSlate(AlignContent));
	MyFlexBox->SetGap(FVector2f((float)Gap.X, (float)Gap.Y));
}

UFlexBoxSlot* UFlexBox::AddChildToFlexBox(UWidget* Content)
{
	return Cast<UFlexBoxSlot>(Super::AddChild(Content));
}

EArcFlexDirection UFlexBox::GetDirection() const
{
	return Direction;
}

void UFlexBox::SetDirection(EArcFlexDirection InDirection)
{
	Direction = InDirection;
	if (MyFlexBox.IsValid())
	{
		MyFlexBox->SetDirection(UFlexBoxPrivate::ToSlate(InDirection));
	}
}

EArcFlexWrap UFlexBox::GetWrap() const
{
	return Wrap;
}

void UFlexBox::SetWrap(EArcFlexWrap InWrap)
{
	Wrap = InWrap;
	if (MyFlexBox.IsValid())
	{
		MyFlexBox->SetWrap(UFlexBoxPrivate::ToSlate(InWrap));
	}
}

EArcFlexJustify UFlexBox::GetJustifyContent() const
{
	return JustifyContent;
}

void UFlexBox::SetJustifyContent(EArcFlexJustify InJustifyContent)
{
	JustifyContent = InJustifyContent;
	if (MyFlexBox.IsValid())
	{
		MyFlexBox->SetJustifyContent(UFlexBoxPrivate::ToSlate(InJustifyContent));
	}
}

EArcFlexAlign UFlexBox::GetAlignItems() const
{
	return AlignItems;
}

void UFlexBox::SetAlignItems(EArcFlexAlign InAlignItems)
{
	AlignItems = InAlignItems;
	if (MyFlexBox.IsValid())
	{
		MyFlexBox->SetAlignItems(UFlexBoxPrivate::ToSlate(InAlignItems));
	}
}

EArcFlexAlign UFlexBox::GetAlignContent() const
{
	return AlignContent;
}

void UFlexBox::SetAlignContent(EArcFlexAlign InAlignContent)
{
	AlignContent = InAlignContent;
	if (MyFlexBox.IsValid())
	{
		MyFlexBox->SetAlignContent(UFlexBoxPrivate::ToSlate(InAlignContent));
	}
}

FVector2D UFlexBox::GetGap() const
{
	return Gap;
}

void UFlexBox::SetGap(FVector2D InGap)
{
	Gap = InGap;
	if (MyFlexBox.IsValid())
	{
		MyFlexBox->SetGap(FVector2f((float)InGap.X, (float)InGap.Y));
	}
}
