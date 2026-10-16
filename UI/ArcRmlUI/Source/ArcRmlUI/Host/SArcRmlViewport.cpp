// Copyright Lukasz Baran. All Rights Reserved.

#include "Host/SArcRmlViewport.h"

#include "ArcRmlUILog.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputSettings.h"
#include "GenericPlatform/ITextInputMethodSystem.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Host/ArcRmlTextInput.h"
#include "InputCoreTypes.h"
#include "Render/ArcRmlSlateElement.h"
#include "Rendering/DrawElements.h"
#include "RenderingThread.h"
#include "SceneView.h"
#include "Slate/SceneViewport.h"
#include "Thread/ArcRmlUIThread.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

#include "RmlUi/Core/Input.h"

namespace ArcRmlViewportPrivate
{
	const TMap<FKey, int32>& GetKeyMap()
	{
		static TMap<FKey, int32> KeyMap;
		if (KeyMap.Num() > 0)
		{
			return KeyMap;
		}

		for (int32 Letter = 0; Letter < 26; ++Letter)
		{
			const FString Name = FString::Chr(static_cast<TCHAR>(TEXT('A') + Letter));
			KeyMap.Add(FKey(FName(*Name)), Rml::Input::KI_A + Letter);
		}

		const FKey Digits[10] = {EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine};
		const FKey NumPad[10] = {EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour,
			EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine};
		for (int32 Index = 0; Index < 10; ++Index)
		{
			KeyMap.Add(Digits[Index], Rml::Input::KI_0 + Index);
			KeyMap.Add(NumPad[Index], Rml::Input::KI_NUMPAD0 + Index);
		}

		const FKey Functions[12] = {EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9,
			EKeys::F10, EKeys::F11, EKeys::F12};
		for (int32 Index = 0; Index < 12; ++Index)
		{
			KeyMap.Add(Functions[Index], Rml::Input::KI_F1 + Index);
		}

		KeyMap.Add(EKeys::SpaceBar, Rml::Input::KI_SPACE);
		KeyMap.Add(EKeys::BackSpace, Rml::Input::KI_BACK);
		KeyMap.Add(EKeys::Tab, Rml::Input::KI_TAB);
		KeyMap.Add(EKeys::Enter, Rml::Input::KI_RETURN);
		KeyMap.Add(EKeys::Pause, Rml::Input::KI_PAUSE);
		KeyMap.Add(EKeys::CapsLock, Rml::Input::KI_CAPITAL);
		KeyMap.Add(EKeys::Escape, Rml::Input::KI_ESCAPE);
		KeyMap.Add(EKeys::PageUp, Rml::Input::KI_PRIOR);
		KeyMap.Add(EKeys::PageDown, Rml::Input::KI_NEXT);
		KeyMap.Add(EKeys::End, Rml::Input::KI_END);
		KeyMap.Add(EKeys::Home, Rml::Input::KI_HOME);
		KeyMap.Add(EKeys::Left, Rml::Input::KI_LEFT);
		KeyMap.Add(EKeys::Up, Rml::Input::KI_UP);
		KeyMap.Add(EKeys::Right, Rml::Input::KI_RIGHT);
		KeyMap.Add(EKeys::Down, Rml::Input::KI_DOWN);
		KeyMap.Add(EKeys::Insert, Rml::Input::KI_INSERT);
		KeyMap.Add(EKeys::Delete, Rml::Input::KI_DELETE);
		KeyMap.Add(EKeys::Multiply, Rml::Input::KI_MULTIPLY);
		KeyMap.Add(EKeys::Add, Rml::Input::KI_ADD);
		KeyMap.Add(EKeys::Subtract, Rml::Input::KI_SUBTRACT);
		KeyMap.Add(EKeys::Decimal, Rml::Input::KI_DECIMAL);
		KeyMap.Add(EKeys::Divide, Rml::Input::KI_DIVIDE);
		KeyMap.Add(EKeys::NumLock, Rml::Input::KI_NUMLOCK);
		KeyMap.Add(EKeys::ScrollLock, Rml::Input::KI_SCROLL);
		KeyMap.Add(EKeys::LeftShift, Rml::Input::KI_LSHIFT);
		KeyMap.Add(EKeys::RightShift, Rml::Input::KI_RSHIFT);
		KeyMap.Add(EKeys::LeftControl, Rml::Input::KI_LCONTROL);
		KeyMap.Add(EKeys::RightControl, Rml::Input::KI_RCONTROL);
		KeyMap.Add(EKeys::LeftAlt, Rml::Input::KI_LMENU);
		KeyMap.Add(EKeys::RightAlt, Rml::Input::KI_RMENU);
		KeyMap.Add(EKeys::LeftCommand, Rml::Input::KI_LMETA);
		KeyMap.Add(EKeys::RightCommand, Rml::Input::KI_RMETA);
		KeyMap.Add(EKeys::Semicolon, Rml::Input::KI_OEM_1);
		KeyMap.Add(EKeys::Equals, Rml::Input::KI_OEM_PLUS);
		KeyMap.Add(EKeys::Comma, Rml::Input::KI_OEM_COMMA);
		KeyMap.Add(EKeys::Hyphen, Rml::Input::KI_OEM_MINUS);
		KeyMap.Add(EKeys::Period, Rml::Input::KI_OEM_PERIOD);
		KeyMap.Add(EKeys::Slash, Rml::Input::KI_OEM_2);
		KeyMap.Add(EKeys::Tilde, Rml::Input::KI_OEM_3);
		KeyMap.Add(EKeys::LeftBracket, Rml::Input::KI_OEM_4);
		KeyMap.Add(EKeys::Backslash, Rml::Input::KI_OEM_5);
		KeyMap.Add(EKeys::RightBracket, Rml::Input::KI_OEM_6);
		KeyMap.Add(EKeys::Apostrophe, Rml::Input::KI_OEM_7);
		return KeyMap;
	}

	int32 ToRmlKey(const FKey& Key)
	{
		const int32* Found = GetKeyMap().Find(Key);
		return Found ? *Found : Rml::Input::KI_UNKNOWN;
	}

	int32 ToRmlModifiers(const FModifierKeysState& Modifiers)
	{
		int32 Result = 0;
		if (Modifiers.IsControlDown())
		{
			Result |= Rml::Input::KM_CTRL;
		}
		if (Modifiers.IsShiftDown())
		{
			Result |= Rml::Input::KM_SHIFT;
		}
		if (Modifiers.IsAltDown())
		{
			Result |= Rml::Input::KM_ALT;
		}
		if (Modifiers.IsCommandDown())
		{
			Result |= Rml::Input::KM_META;
		}
		if (Modifiers.AreCapsLocked())
		{
			Result |= Rml::Input::KM_CAPSLOCK;
		}
		return Result;
	}

	int32 ToRmlButton(const FKey& Button)
	{
		if (Button == EKeys::LeftMouseButton)
		{
			return 0;
		}
		if (Button == EKeys::RightMouseButton)
		{
			return 1;
		}
		if (Button == EKeys::MiddleMouseButton)
		{
			return 2;
		}
		return INDEX_NONE;
	}

	EMouseCursor::Type ToMouseCursor(const FString& CursorName)
	{
		if (CursorName == TEXT("pointer") || CursorName == TEXT("hand"))
		{
			return EMouseCursor::Hand;
		}
		if (CursorName == TEXT("text"))
		{
			return EMouseCursor::TextEditBeam;
		}
		if (CursorName == TEXT("move"))
		{
			return EMouseCursor::CardinalCross;
		}
		if (CursorName == TEXT("ew-resize") || CursorName == TEXT("col-resize"))
		{
			return EMouseCursor::ResizeLeftRight;
		}
		if (CursorName == TEXT("ns-resize") || CursorName == TEXT("row-resize"))
		{
			return EMouseCursor::ResizeUpDown;
		}
		if (CursorName == TEXT("not-allowed"))
		{
			return EMouseCursor::SlashedCircle;
		}
		if (CursorName == TEXT("grab"))
		{
			return EMouseCursor::GrabHand;
		}
		if (CursorName == TEXT("grabbing"))
		{
			return EMouseCursor::GrabHandClosed;
		}
		if (CursorName == TEXT("crosshair"))
		{
			return EMouseCursor::Crosshairs;
		}
		return EMouseCursor::Default;
	}
}

SArcRmlViewport::SArcRmlViewport()
{
	SetCanTick(true);
}

SArcRmlViewport::~SArcRmlViewport()
{
	ReleaseTextInput();
	ReleaseView();

	if (Element.IsValid())
	{
		ENQUEUE_RENDER_COMMAND(ArcRmlReleaseViewportElement)(
			[ReleasedElement = MoveTemp(Element)](FRHICommandListImmediate&) mutable
			{
				ReleasedElement.Reset();
			});
	}
}

void SArcRmlViewport::ReleaseView()
{
	if (bViewReleased)
	{
		return;
	}

	bViewReleased = true;
	OnCloseKey.Unbind();
	OnRootTransitionEnd.Unbind();
	if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
	{
		PinnedThread->RemoveView(ViewId);
	}
}

void SArcRmlViewport::SetViewPaused(bool bPaused)
{
	if (bViewReleased)
	{
		return;
	}

	if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
	{
		PinnedThread->SetViewPaused(ViewId, bPaused);
	}
}

void SArcRmlViewport::SetRootClass(const FString& Class, bool bSet)
{
	if (bViewReleased)
	{
		return;
	}

	if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
	{
		++RootClassSerial;
		PinnedThread->SetRootClass(ViewId, Class, bSet);
	}
}

void SArcRmlViewport::SetOnRootTransitionEnd(FSimpleDelegate&& InOnRootTransitionEnd)
{
	OnRootTransitionEnd = MoveTemp(InOnRootTransitionEnd);
}

void SArcRmlViewport::Construct(const FArguments& InArgs, const TSharedRef<FArcRmlUIThread>& InThread, ULocalPlayer* InLocalPlayer)
{
	Thread = InThread;
	LocalPlayer = InLocalPlayer;
	Element = MakeShared<FArcRmlSlateElement, ESPMode::ThreadSafe>();
	Channel = MakeShared<FArcRmlViewChannel, ESPMode::ThreadSafe>();
	ForceVolatile(true);

	ViewId = InThread->AddView(Element.ToSharedRef(), Channel.ToSharedRef());
}

uint32 SArcRmlViewport::GetViewId() const
{
	return ViewId;
}

void SArcRmlViewport::LoadDocument(const FString& Path, FIntPoint InPixelSize, float InPixelScale, double OpenSeconds)
{
	PixelSize = InPixelSize;
	PixelScale = InPixelScale;
	UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: view %u load size %dx%d scale %.2f", ViewId, PixelSize.X, PixelSize.Y, PixelScale);
	if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
	{
		PinnedThread->LoadDocument(ViewId, Path, PixelSize, PixelScale, OpenSeconds);
	}
}

void SArcRmlViewport::EmbedDocument(const FString& Path)
{
	if (bViewReleased)
	{
		return;
	}

	if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
	{
		PinnedThread->EmbedDocument(ViewId, Path);
	}
}

void SArcRmlViewport::SetOnViewModelCall(FArcRmlOnViewModelCall InOnViewModelCall)
{
	OnViewModelCall = MoveTemp(InOnViewModelCall);
}

void SArcRmlViewport::SetKeyPolicy(EArcRmlInputMode InInputMode, TArray<FKey>&& InBackKeys, TArray<FKey>&& InCloseKeys, FSimpleDelegate&& InOnCloseKey)
{
	InputMode = InInputMode;
	BackKeys = MoveTemp(InBackKeys);
	CloseKeys = MoveTemp(InCloseKeys);
	OnCloseKey = MoveTemp(InOnCloseKey);
}

void SArcRmlViewport::PostInputEvent(FArcRmlInputEvent&& Event)
{
	Event.ViewId = ViewId;
	if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
	{
		PinnedThread->PostInput(MoveTemp(Event));
	}
}

FVector2D SArcRmlViewport::ViewPixelsToScreen(FVector2f ViewPixels) const
{
	const FVector2f Local = ViewPixels / FMath::Max(PixelScale, UE_SMALL_NUMBER);
	return GetTickSpaceGeometry().LocalToAbsolute(Local);
}

TSharedPtr<FGenericWindow> SArcRmlViewport::GetNativeWindow() const
{
	if (!FSlateApplication::IsInitialized())
	{
		return nullptr;
	}

	const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
	return Window.IsValid() ? Window->GetNativeWindow() : nullptr;
}

FVector2D SArcRmlViewport::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D::ZeroVector;
}

int32 SArcRmlViewport::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2f TopLeft = AllottedGeometry.LocalToAbsolute(FVector2f::ZeroVector);
	const FVector2f BottomRight = AllottedGeometry.LocalToAbsolute(AllottedGeometry.GetLocalSize());
	const FIntRect Destination(
		FMath::RoundToInt(TopLeft.X), FMath::RoundToInt(TopLeft.Y),
		FMath::RoundToInt(BottomRight.X), FMath::RoundToInt(BottomRight.Y));

	ENQUEUE_RENDER_COMMAND(ArcRmlSetViewportDestination)(
		[PaintedElement = Element, Destination](FRHICommandListImmediate&)
		{
			PaintedElement->SetDestination_RenderThread(Destination);
		});

	FSlateDrawElement::MakeCustom(OutDrawElements, LayerId, Element);
	return LayerId;
}

void SArcRmlViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const FVector2f AbsoluteSize = AllottedGeometry.GetAbsoluteSize();
	const FIntPoint NewPixelSize(FMath::RoundToInt(AbsoluteSize.X), FMath::RoundToInt(AbsoluteSize.Y));
	const float NewScale = AllottedGeometry.Scale;
	if (NewPixelSize != PixelSize || !FMath::IsNearlyEqual(NewScale, PixelScale))
	{
		PixelSize = NewPixelSize;
		PixelScale = NewScale;
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: view %u size %dx%d scale %.2f", ViewId, PixelSize.X, PixelSize.Y, PixelScale);
		if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
		{
			PinnedThread->ResizeView(ViewId, PixelSize, PixelScale);
		}
	}

	PublishCamera(AllottedGeometry);
	Channel->ReadSnapshot(Snapshot, SnapshotSerial);

	// The root transition handler may release the owner's last reference to this widget.
	const TSharedRef<SArcRmlViewport> KeepAlive = SharedThis(this);
	FArcRmlOutboundEvent Outbound;
	while (Channel->PopOutbound(Outbound))
	{
		if (Outbound.Type == EArcRmlOutboundType::ClipboardCopy)
		{
			FPlatformApplicationMisc::ClipboardCopy(*Outbound.Text);
		}
		else if (Outbound.Type == EArcRmlOutboundType::ViewModelCall && !bViewReleased)
		{
			OnViewModelCall.ExecuteIfBound(Outbound.Call);
		}
		else if (Outbound.Type == EArcRmlOutboundType::RootTransitionEnd && Outbound.RootClassSerial == RootClassSerial)
		{
			OnRootTransitionEnd.ExecuteIfBound();
		}
		else if (Outbound.Type == EArcRmlOutboundType::AssetTextureRequest)
		{
			if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
			{
				PinnedThread->RequestAssetTexture(ViewId, Outbound.Text, Outbound.AssetTexture);
			}
		}
	}

	UpdateTextInput();
}

void SArcRmlViewport::PublishCamera(const FGeometry& AllottedGeometry)
{
	FArcRmlViewCamera Camera;
	const ULocalPlayer* Player = LocalPlayer.Get();
	UGameViewportClient* ViewportClient = Player ? Player->ViewportClient.Get() : nullptr;
	const TSharedPtr<SViewport> GameViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
	FSceneViewProjectionData ProjectionData;
	if (GameViewportWidget.IsValid() && Player->GetProjectionData(ViewportClient->Viewport, ProjectionData))
	{
		Camera.bValid = true;
		Camera.ViewOrigin = ProjectionData.ViewOrigin;
		Camera.TranslatedViewProjection = ProjectionData.ViewRotationMatrix * ProjectionData.ProjectionMatrix;
		Camera.ViewRect = ProjectionData.GetConstrainedViewRect();
		Camera.ContentOrigin = FVector2D(AllottedGeometry.GetAbsolutePosition() - GameViewportWidget->GetTickSpaceGeometry().GetAbsolutePosition());
	}

	if (Camera == PublishedCamera)
	{
		return;
	}

	PublishedCamera = Camera;
	Channel->PublishCamera(Camera);
	if (const TSharedPtr<FArcRmlUIThread> PinnedThread = Thread.Pin())
	{
		PinnedThread->Wake();
	}
}

bool SArcRmlViewport::SupportsKeyboardFocus() const
{
	return true;
}

FVector2f SArcRmlViewport::ToViewPixels(const FGeometry& Geometry, const FVector2D& ScreenPosition) const
{
	const FVector2f Local = Geometry.AbsoluteToLocal(ScreenPosition);
	return Local * PixelScale;
}

bool SArcRmlViewport::IsInteractiveAt(FVector2f ViewPixels) const
{
	const FIntPoint Point(FMath::FloorToInt(ViewPixels.X), FMath::FloorToInt(ViewPixels.Y));
	for (const FIntRect& Rect : Snapshot.InteractiveRects)
	{
		if (Rect.Contains(Point))
		{
			return true;
		}
	}
	return false;
}

void SArcRmlViewport::PostPointerEvent(EArcRmlInputType Type, const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 Button)
{
	FArcRmlInputEvent Event;
	Event.Type = Type;
	Event.Position = ToViewPixels(Geometry, PointerEvent.GetScreenSpacePosition());
	Event.Button = Button;
	Event.WheelDelta = PointerEvent.GetWheelDelta();
	Event.Modifiers = ArcRmlViewportPrivate::ToRmlModifiers(PointerEvent.GetModifierKeys());
	PostInputEvent(MoveTemp(Event));
}

FReply SArcRmlViewport::HandlePointerDown(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 Button)
{
	if (Button == INDEX_NONE || !IsInteractiveAt(ToViewPixels(Geometry, PointerEvent.GetScreenSpacePosition())))
	{
		return FReply::Unhandled();
	}

	PostPointerEvent(EArcRmlInputType::MouseDown, Geometry, PointerEvent, Button);
	bPointerCaptured = true;
	return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
}

FReply SArcRmlViewport::HandlePointerUp(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 Button)
{
	if (!bPointerCaptured || Button == INDEX_NONE)
	{
		return FReply::Unhandled();
	}

	PostPointerEvent(EArcRmlInputType::MouseUp, Geometry, PointerEvent, Button);
	bPointerCaptured = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SArcRmlViewport::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HandlePointerDown(MyGeometry, MouseEvent, ArcRmlViewportPrivate::ToRmlButton(MouseEvent.GetEffectingButton()));
}

FReply SArcRmlViewport::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return HandlePointerUp(MyGeometry, MouseEvent, ArcRmlViewportPrivate::ToRmlButton(MouseEvent.GetEffectingButton()));
}

FReply SArcRmlViewport::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return HandlePointerDown(InMyGeometry, InMouseEvent, ArcRmlViewportPrivate::ToRmlButton(InMouseEvent.GetEffectingButton()));
}

FReply SArcRmlViewport::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	PostPointerEvent(EArcRmlInputType::MouseMove, MyGeometry, MouseEvent, 0);
	const bool bInteractive = bPointerCaptured || IsInteractiveAt(ToViewPixels(MyGeometry, MouseEvent.GetScreenSpacePosition()));
	return bInteractive ? FReply::Handled() : FReply::Unhandled();
}

FReply SArcRmlViewport::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!IsInteractiveAt(ToViewPixels(MyGeometry, MouseEvent.GetScreenSpacePosition())))
	{
		return FReply::Unhandled();
	}

	PostPointerEvent(EArcRmlInputType::MouseWheel, MyGeometry, MouseEvent, 0);
	return FReply::Handled();
}

void SArcRmlViewport::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SLeafWidget::OnMouseLeave(MouseEvent);

	FArcRmlInputEvent Event;
	Event.Type = EArcRmlInputType::MouseLeave;
	PostInputEvent(MoveTemp(Event));
}

void SArcRmlViewport::OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
	bPointerCaptured = false;
}

FCursorReply SArcRmlViewport::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const
{
	if (!bPointerCaptured && !IsInteractiveAt(ToViewPixels(MyGeometry, CursorEvent.GetScreenSpacePosition())))
	{
		return FCursorReply::Unhandled();
	}
	return FCursorReply::Cursor(ArcRmlViewportPrivate::ToMouseCursor(Snapshot.Cursor));
}

FReply SArcRmlViewport::HandleKey(EArcRmlInputType Type, const FKeyEvent& KeyEvent)
{
	const FKey& PressedKey = KeyEvent.GetKey();
	const bool bLogKey = Type == EArcRmlInputType::KeyDown && !KeyEvent.IsRepeat();
	if (InputMode != EArcRmlInputMode::Game)
	{
		if (CloseKeys.Contains(PressedKey))
		{
			if (bLogKey)
			{
				UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: view %u key %ls: close key, closing", ViewId, *PressedKey.ToString());
				OnCloseKey.ExecuteIfBound();
			}
			return FReply::Handled();
		}

		if (BackKeys.Contains(PressedKey))
		{
			if (bLogKey)
			{
				UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: view %u key %ls: back key, left unhandled for the CommonUI back action", ViewId, *PressedKey.ToString());
			}
			return FReply::Unhandled();
		}
	}

	const bool bForwardKeys = InputMode == EArcRmlInputMode::Menu || Snapshot.bHasFocusedElement;
	const int32 Key = ArcRmlViewportPrivate::ToRmlKey(PressedKey);
	if (!bForwardKeys || Key == Rml::Input::KI_UNKNOWN || GetDefault<UInputSettings>()->ConsoleKeys.Contains(PressedKey))
	{
		if (bLogKey)
		{
			UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: view %u key %ls: unhandled (forward %ls, rml key %d)", ViewId, *PressedKey.ToString(),
				bForwardKeys ? TEXT("true") : TEXT("false"), Key);
		}
		return FReply::Unhandled();
	}

	if (bLogKey)
	{
		UE_LOGF(LogArcRmlUI, Log, "ArcRmlUI: view %u key %ls: forwarded to RmlUi", ViewId, *PressedKey.ToString());
	}

	FArcRmlInputEvent Event;
	Event.Type = Type;
	Event.Key = Key;
	Event.Modifiers = ArcRmlViewportPrivate::ToRmlModifiers(KeyEvent.GetModifierKeys());
	PostInputEvent(MoveTemp(Event));
	return FReply::Handled();
}

FReply SArcRmlViewport::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return HandleKey(EArcRmlInputType::KeyDown, InKeyEvent);
}

FReply SArcRmlViewport::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return HandleKey(EArcRmlInputType::KeyUp, InKeyEvent);
}

FReply SArcRmlViewport::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	if (InputMode != EArcRmlInputMode::Menu && !Snapshot.TextField.bActive)
	{
		return FReply::Unhandled();
	}

	if (TextInputContext.IsValid() && TextInputContext->IsComposing())
	{
		return FReply::Handled();
	}

	const TCHAR Character = InCharacterEvent.GetCharacter();
	if (Character < 32 || Character == 127)
	{
		return FReply::Unhandled();
	}

	FArcRmlInputEvent Event;
	Event.Type = EArcRmlInputType::TextInput;
	Event.Text = FString::Chr(Character);
	PostInputEvent(MoveTemp(Event));
	return FReply::Handled();
}

FReply SArcRmlViewport::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return HandlePointerDown(MyGeometry, InTouchEvent, 0);
}

FReply SArcRmlViewport::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	PostPointerEvent(EArcRmlInputType::MouseMove, MyGeometry, InTouchEvent, 0);
	return bPointerCaptured ? FReply::Handled() : FReply::Unhandled();
}

FReply SArcRmlViewport::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return HandlePointerUp(MyGeometry, InTouchEvent, 0);
}

void SArcRmlViewport::UpdateTextInput()
{
	ITextInputMethodSystem* TextInputSystem = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetTextInputMethodSystem() : nullptr;
	if (!TextInputSystem)
	{
		return;
	}

	if (!TextInputContext.IsValid())
	{
		TextInputContext = MakeShared<FArcRmlTextInputContext>(SharedThis(this));
		TextInputNotifier = TextInputSystem->RegisterContext(TextInputContext.ToSharedRef());
	}

	const bool bWantsTextInput = Snapshot.TextField.bActive && HasKeyboardFocus();
	if (bWantsTextInput && TextInputContext->ApplyShadow(Snapshot.TextField) && TextInputNotifier.IsValid())
	{
		TextInputNotifier->NotifySelectionChanged();
		TextInputNotifier->NotifyLayoutChanged(ITextInputMethodChangeNotifier::ELayoutChangeType::Changed);
	}

	if (bWantsTextInput != bTextInputActive)
	{
		if (bWantsTextInput)
		{
			TextInputSystem->ActivateContext(TextInputContext.ToSharedRef());
		}
		else
		{
			TextInputSystem->DeactivateContext(TextInputContext.ToSharedRef());
		}
		bTextInputActive = bWantsTextInput;
	}
}

void SArcRmlViewport::ReleaseTextInput()
{
	if (!TextInputContext.IsValid())
	{
		return;
	}

	ITextInputMethodSystem* TextInputSystem = FSlateApplication::IsInitialized() ? FSlateApplication::Get().GetTextInputMethodSystem() : nullptr;
	if (TextInputSystem)
	{
		if (bTextInputActive)
		{
			TextInputSystem->DeactivateContext(TextInputContext.ToSharedRef());
		}
		TextInputSystem->UnregisterContext(TextInputContext.ToSharedRef());
	}
	bTextInputActive = false;
	TextInputNotifier.Reset();
	TextInputContext.Reset();
}
