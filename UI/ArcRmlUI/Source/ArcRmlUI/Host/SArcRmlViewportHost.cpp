// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/SArcRmlViewportHost.h"

#include "Framework/Application/SlateApplication.h"
#include "Host/ArcRmlContentRect.h"
#include "Host/SArcRmlViewport.h"
#include "Misc/CoreDelegates.h"
#include "Widgets/SNullWidget.h"

SArcRmlViewportHost::~SArcRmlViewportHost()
{
	FCoreDelegates::OnSafeFrameChangedEvent.Remove(SafeFrameChangedHandle);
#if WITH_EDITOR
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().OnDebugSafeZoneChanged.Remove(DebugSafeZoneChangedHandle);
	}
#endif
}

void SArcRmlViewportHost::Construct(const FArguments& InArgs)
{
	check(InArgs._MaxContentAspect.IsBound());
	MaxContentAspect = InArgs._MaxContentAspect;
	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNullWidget::NullWidget
	];

	SafeFrameChangedHandle = FCoreDelegates::OnSafeFrameChangedEvent.AddSP(this, &SArcRmlViewportHost::MarkSafeMarginDirty);
#if WITH_EDITOR
	DebugSafeZoneChangedHandle = FSlateApplication::Get().OnDebugSafeZoneChanged.AddSP(this, &SArcRmlViewportHost::HandleDebugSafeZoneChanged);
#endif
}

void SArcRmlViewportHost::SetViewport(const TSharedPtr<SArcRmlViewport>& InViewport)
{
	Viewport = InViewport;
	if (InViewport.IsValid())
	{
		ChildSlot[InViewport.ToSharedRef()];
	}
	else
	{
		ChildSlot[SNullWidget::NullWidget];
	}
}

bool SArcRmlViewportHost::HasViewport() const
{
	return Viewport.IsValid();
}

void SArcRmlViewportHost::MarkSafeMarginDirty()
{
	bSafeMarginDirty = true;
}

#if WITH_EDITOR
void SArcRmlViewportHost::HandleDebugSafeZoneChanged(const FMargin& NewSafeZone, bool bShouldRecacheMetrics)
{
	bSafeMarginDirty = true;
}
#endif

void SArcRmlViewportHost::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const TSharedPtr<SArcRmlViewport> Pinned = Viewport.Pin();
	if (!Pinned.IsValid())
	{
		return;
	}

	const FVector2f HostSize = AllottedGeometry.GetAbsoluteSize();
	if (bSafeMarginDirty || HostSize != SafeMarginHostSize)
	{
		SafeMargin = ArcRmlContentRect::GetSafeMargin();
		SafeMarginHostSize = HostSize;
		bSafeMarginDirty = false;
	}

	const FVector2f HostPosition = AllottedGeometry.GetAbsolutePosition();
	const FSlateRect HostRect(HostPosition, HostPosition + HostSize);
	const FSlateRect ContentRect = ArcRmlContentRect::Compute(HostRect, SafeMargin, MaxContentAspect.Get());
	const float InverseScale = 1.0f / AllottedGeometry.Scale;
	const FMargin Padding(
		(ContentRect.Left - HostRect.Left) * InverseScale,
		(ContentRect.Top - HostRect.Top) * InverseScale,
		(HostRect.Right - ContentRect.Right) * InverseScale,
		(HostRect.Bottom - ContentRect.Bottom) * InverseScale);
	if (Padding != ChildSlot.GetPadding())
	{
		ChildSlot.SetPadding(Padding);
	}
}

bool SArcRmlViewportHost::SupportsKeyboardFocus() const
{
	return true;
}

FReply SArcRmlViewportHost::OnFocusReceived(const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent)
{
	const TSharedPtr<SArcRmlViewport> Pinned = Viewport.Pin();
	if (!Pinned.IsValid())
	{
		return FReply::Unhandled();
	}
	return FReply::Handled().SetUserFocus(Pinned.ToSharedRef(), InFocusEvent.GetCause());
}
