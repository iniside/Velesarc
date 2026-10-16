// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/ArcRmlViewportWidget.h"

#include "Engine/LocalPlayer.h"
#include "Host/ArcRmlViewportSubsystem.h"
#include "Host/SArcRmlViewport.h"
#include "Host/SArcRmlViewportHost.h"

void UArcRmlViewportWidget::SetViewport(const TSharedPtr<SArcRmlViewport>& InViewport)
{
	Viewport = InViewport;
	if (Host.IsValid())
	{
		Host->SetViewport(Viewport);
	}
}

bool UArcRmlViewportWidget::HasHostedViewport() const
{
	return Host.IsValid() && Host->HasViewport();
}

void UArcRmlViewportWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	if (Host.IsValid())
	{
		Host->SetViewport(nullptr);
	}
	Host.Reset();
	Viewport.Reset();
}

TSharedRef<SWidget> UArcRmlViewportWidget::RebuildWidget()
{
	const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	check(LocalPlayer);
	UArcRmlViewportSubsystem* Subsystem = LocalPlayer->GetSubsystem<UArcRmlViewportSubsystem>();
	check(Subsystem);
	Host = SNew(SArcRmlViewportHost)
		.MaxContentAspect(TAttribute<float>::Create(TAttribute<float>::FGetter::CreateUObject(Subsystem, &UArcRmlViewportSubsystem::ReadMaxContentAspect)));
	Host->SetViewport(Viewport);
	return Host.ToSharedRef();
}
