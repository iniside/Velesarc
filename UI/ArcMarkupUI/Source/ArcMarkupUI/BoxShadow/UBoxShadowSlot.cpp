// Copyright Lukasz Baran. All Rights Reserved.

#include "BoxShadow/UBoxShadowSlot.h"
#include "Components/Widget.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SOverlay.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UBoxShadowSlot)

namespace UBoxShadowSlotPrivate
{
	// Content is layered above the shadow (shadow uses ZOrder 0).
	static constexpr int32 ContentZOrder = 1;
}

void UBoxShadowSlot::BuildSlot(TSharedRef<SOverlay> InOverlay)
{
	Overlay = InOverlay;

	InOverlay->AddSlot(UBoxShadowSlotPrivate::ContentZOrder)
		[
			Content == nullptr ? SNullWidget::NullWidget : Content->TakeWidget()
		];
}

void UBoxShadowSlot::SynchronizeProperties()
{
}

void UBoxShadowSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Overlay.Reset();
}
