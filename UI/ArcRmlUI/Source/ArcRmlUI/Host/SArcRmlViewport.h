// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ArcRmlViewModelDeclarations.h"
#include "Thread/ArcRmlUIQueues.h"
#include "Widgets/SLeafWidget.h"

class FArcRmlSlateElement;
class FArcRmlTextInputContext;
class FArcRmlUIThread;
class FGenericWindow;
class ITextInputMethodChangeNotifier;
class ULocalPlayer;

DECLARE_DELEGATE_OneParam(FArcRmlOnViewModelCall, const FArcRmlViewModelCall&);

class ARCRMLUI_API SArcRmlViewport final : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SArcRmlViewport)
	{
	}
	SLATE_END_ARGS()

	SArcRmlViewport();
	virtual ~SArcRmlViewport() override;

	void Construct(const FArguments& InArgs, const TSharedRef<FArcRmlUIThread>& InThread, ULocalPlayer* InLocalPlayer);

	uint32 GetViewId() const;
	void LoadDocument(const FString& Path, FIntPoint InPixelSize, float InPixelScale, double OpenSeconds);
	void EmbedDocument(const FString& Path);
	void ReleaseView();
	void SetViewPaused(bool bPaused);
	void SetRootClass(const FString& Class, bool bSet);
	void SetOnRootTransitionEnd(FSimpleDelegate&& InOnRootTransitionEnd);
	void SetOnViewModelCall(FArcRmlOnViewModelCall InOnViewModelCall);
	void SetKeyPolicy(EArcRmlInputMode InInputMode, TArray<FKey>&& InBackKeys, TArray<FKey>&& InCloseKeys, FSimpleDelegate&& InOnCloseKey);
	void PostInputEvent(FArcRmlInputEvent&& Event);
	FVector2D ViewPixelsToScreen(FVector2f ViewPixels) const;
	TSharedPtr<FGenericWindow> GetNativeWindow() const;

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override;

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) const override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;

private:
	FVector2f ToViewPixels(const FGeometry& Geometry, const FVector2D& ScreenPosition) const;
	bool IsInteractiveAt(FVector2f ViewPixels) const;
	void PostPointerEvent(EArcRmlInputType Type, const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 Button);
	FReply HandlePointerDown(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 Button);
	FReply HandlePointerUp(const FGeometry& Geometry, const FPointerEvent& PointerEvent, int32 Button);
	FReply HandleKey(EArcRmlInputType Type, const FKeyEvent& KeyEvent);
	void UpdateTextInput();
	void PublishCamera(const FGeometry& AllottedGeometry);
	void ReleaseTextInput();

	TWeakPtr<FArcRmlUIThread> Thread;
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;
	FArcRmlViewCamera PublishedCamera;
	uint32 ViewId = 0;
	FArcRmlOnViewModelCall OnViewModelCall;
	EArcRmlInputMode InputMode = EArcRmlInputMode::Game;
	TArray<FKey> BackKeys;
	TArray<FKey> CloseKeys;
	FSimpleDelegate OnCloseKey;
	FSimpleDelegate OnRootTransitionEnd;
	uint32 RootClassSerial = 0;
	TSharedPtr<FArcRmlSlateElement, ESPMode::ThreadSafe> Element;
	TSharedPtr<FArcRmlViewChannel, ESPMode::ThreadSafe> Channel;
	FArcRmlViewSnapshot Snapshot;
	uint64 SnapshotSerial = 0;
	FIntPoint PixelSize = FIntPoint::ZeroValue;
	float PixelScale = 1.0f;
	bool bPointerCaptured = false;
	bool bViewReleased = false;
	TSharedPtr<FArcRmlTextInputContext> TextInputContext;
	TSharedPtr<ITextInputMethodChangeNotifier> TextInputNotifier;
	bool bTextInputActive = false;
};
