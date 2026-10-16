// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/ArcRmlActivatableWidget.h"

#include "ArcRmlUILog.h"
#include "Blueprint/WidgetTree.h"
#include "CommonInputModeTypes.h"
#include "Host/ArcRmlViewportSubsystem.h"
#include "Host/ArcRmlViewportWidget.h"
#include "Host/SArcRmlViewport.h"
#include "Input/UIActionBindingHandle.h"

void UArcRmlActivatableWidget::InitDocument(UArcRmlViewportSubsystem& InOwner, const FArcRmlDocumentRef& InDocument, const FArcRmlDocumentMeta& InMeta,
	const TSharedRef<SArcRmlViewport>& InViewport)
{
	Owner = &InOwner;
	Document = InDocument;
	Meta = InMeta;
	Viewport = InViewport;
	if (ViewportWidget)
	{
		ViewportWidget->SetViewport(InViewport);
	}
	bIsBackHandler = Meta.InputMode != EArcRmlInputMode::Game;
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

TOptional<FUIInputConfig> UArcRmlActivatableWidget::GetDesiredInputConfig() const
{
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' input config requested by CommonUI (input-mode %ls)", *Document.Path,
		Meta.InputMode == EArcRmlInputMode::Game ? TEXT("game") : Meta.InputMode == EArcRmlInputMode::Menu ? TEXT("menu") : TEXT("all"));
	switch (Meta.InputMode)
	{
	case EArcRmlInputMode::Menu:
		return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
	case EArcRmlInputMode::All:
		return FUIInputConfig(ECommonInputMode::All, EMouseCaptureMode::NoCapture);
	case EArcRmlInputMode::Game:
		break;
	}
	return FUIInputConfig(ECommonInputMode::Game, EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown, true);
}

void UArcRmlActivatableWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ViewportWidget = WidgetTree->ConstructWidget<UArcRmlViewportWidget>();
	ViewportWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = ViewportWidget;
}

TSharedRef<SWidget> UArcRmlActivatableWidget::RebuildWidget()
{
	if (ViewportWidget)
	{
		ViewportWidget->SetViewport(Viewport.Pin());
	}
	return Super::RebuildWidget();
}

UWidget* UArcRmlActivatableWidget::NativeGetDesiredFocusTarget() const
{
	UWidget* Target = Meta.InputMode == EArcRmlInputMode::Game ? nullptr : ViewportWidget.Get();
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' focus target requested by CommonUI: %ls", *Document.Path,
		!Target ? TEXT("none") : !Target->GetCachedWidget().IsValid() ? TEXT("viewport host (Slate widget not built)") : ViewportWidget->HasHostedViewport() ? TEXT("viewport host (focusable, forwards focus to the viewport)") : TEXT("viewport host (focusable, NO viewport attached)"));
	return Target;
}

bool UArcRmlActivatableWidget::NativeOnHandleBackAction()
{
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: '%ls' CommonUI back action received, closing", *Document.Path);
	if (UArcRmlViewportSubsystem* PinnedOwner = Owner.Get())
	{
		const FArcRmlDocumentRef DocumentToClose = Document;
		PinnedOwner->CloseDocument(DocumentToClose);
	}
	return true;
}

void UArcRmlActivatableWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	if (UArcRmlViewportSubsystem* PinnedOwner = Owner.Get())
	{
		PinnedOwner->NotifyActivationChanged(*this, Document, true);
	}
}

void UArcRmlActivatableWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
	if (UArcRmlViewportSubsystem* PinnedOwner = Owner.Get())
	{
		PinnedOwner->NotifyActivationChanged(*this, Document, false);
	}
}
