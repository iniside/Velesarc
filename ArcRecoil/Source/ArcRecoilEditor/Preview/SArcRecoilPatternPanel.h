// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ITransportControl.h"
#include "Input/Reply.h"
#include "Preview/ArcRecoilPreviewSim.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SArcRecoilPatternCanvas;
class SSlider;
class UArcRecoilPreviewSettings;

class SArcRecoilPatternPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcRecoilPatternPanel)
	{
	}
		SLATE_ARGUMENT(TWeakObjectPtr<UArcRecoilPreviewSettings>, Settings)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void RefreshFromResult(TSharedPtr<const ArcRecoil::Preview::FArcRecoilPreviewResult> InResult);

	void RepaintCanvases();

private:
	FReply HandleForwardPlay();
	FReply HandleForwardStep();
	FReply HandleBackwardStep();
	FReply HandleToggleLooping();
	bool HandleGetLooping() const;
	EPlaybackMode::Type HandleGetPlaybackMode() const;
	void HandleTickPlayback(double InCurrentTime, float InDeltaTime);

	float GetScrubValue() const;
	float GetScrubStepSize() const;
	void HandleScrubValueChanged(float NewValue);
	void HandleScrubCaptureBegin();
	void HandleScrubCaptureEnd();

	float GetPlaybackSpeed() const;
	void HandlePlaybackSpeedChanged(float NewValue);

	FText GetPositionText() const;
	FText GetStatusText() const;

	void ApplyCurrentSample(float InSample);
	int32 GetLastSampleIndex() const;
	bool HasPlayableResult() const;

	TSharedPtr<SArcRecoilPatternCanvas> CameraCanvas;
	TSharedPtr<SArcRecoilPatternCanvas> ImpactCanvas;
	TSharedPtr<SSlider> ScrubSlider;
	TSharedPtr<const ArcRecoil::Preview::FArcRecoilPreviewResult> Result;
	TWeakObjectPtr<UArcRecoilPreviewSettings> Settings;
	float CurrentSample = 0.0f;
	bool bPlaying = false;
	bool bResumePlaybackAfterScrub = false;
};
