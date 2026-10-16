// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/SArcRecoilPatternPanel.h"

#include "Algo/BinarySearch.h"
#include "EditorWidgetsModule.h"
#include "Modules/ModuleManager.h"
#include "Preview/ArcRecoilPreviewSettings.h"
#include "Preview/SArcRecoilPatternCanvas.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcRecoilPatternPanel"

namespace ArcRecoilPatternPanelPrivate
{
	int32 SampleToIndex(const float Sample, const int32 LastIndex)
	{
		return FMath::Clamp(FMath::FloorToInt(FMath::IsFinite(Sample) ? Sample : 0.0f), 0, LastIndex);
	}
}

void SArcRecoilPatternPanel::Construct(const FArguments& InArgs)
{
	Settings = InArgs._Settings;

	FTransportControlArgs TransportArgs;
	TransportArgs.OnForwardPlay = FOnClicked::CreateSP(this, &SArcRecoilPatternPanel::HandleForwardPlay);
	TransportArgs.OnForwardStep = FOnClicked::CreateSP(this, &SArcRecoilPatternPanel::HandleForwardStep);
	TransportArgs.OnBackwardStep = FOnClicked::CreateSP(this, &SArcRecoilPatternPanel::HandleBackwardStep);
	TransportArgs.OnToggleLooping = FOnClicked::CreateSP(this, &SArcRecoilPatternPanel::HandleToggleLooping);
	TransportArgs.OnGetLooping = FOnGetLooping::CreateSP(this, &SArcRecoilPatternPanel::HandleGetLooping);
	TransportArgs.OnGetPlaybackMode = FOnGetPlaybackMode::CreateSP(this, &SArcRecoilPatternPanel::HandleGetPlaybackMode);
	TransportArgs.OnTickPlayback = FOnTickPlayback::CreateSP(this, &SArcRecoilPatternPanel::HandleTickPlayback);

	FEditorWidgetsModule& EditorWidgetsModule =
		FModuleManager::LoadModuleChecked<FEditorWidgetsModule>(TEXT("EditorWidgets"));

	// The transport widget owns the playback timer, so this panel registers none of its own.
	const TSharedRef<ITransportControl> TransportControl = EditorWidgetsModule.CreateTransportControl(TransportArgs);

	UArcRecoilPreviewSettings* CanvasSettings = Settings.Get();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.Padding(2.0f)
			[
				SAssignNew(CameraCanvas, SArcRecoilPatternCanvas)
				.Pane(EArcRecoilPreviewPane::Camera)
				.Settings(CanvasSettings)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.5f)
			.Padding(2.0f)
			[
				SAssignNew(ImpactCanvas, SArcRecoilPatternCanvas)
				.Pane(EArcRecoilPreviewPane::Impact)
				.Settings(CanvasSettings)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				TransportControl
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(8.0f, 0.0f)
			[
				// The scrub range is sample space, not shot space: the last shot's substeps and the whole tail live past the final shot start.
				SAssignNew(ScrubSlider, SSlider)
				.MinValue(0.0f)
				.MaxValue(static_cast<float>(GetLastSampleIndex()))
				.MouseUsesStep(true)
				.StepSize(this, &SArcRecoilPatternPanel::GetScrubStepSize)
				.Value(this, &SArcRecoilPatternPanel::GetScrubValue)
				.OnValueChanged(this, &SArcRecoilPatternPanel::HandleScrubValueChanged)
				.OnMouseCaptureBegin(this, &SArcRecoilPatternPanel::HandleScrubCaptureBegin)
				.OnMouseCaptureEnd(this, &SArcRecoilPatternPanel::HandleScrubCaptureEnd)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(STextBlock)
				.MinDesiredWidth(260.0f)
				.Text(this, &SArcRecoilPatternPanel::GetPositionText)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SpeedLabel", "speed"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(72.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(ArcRecoil::Preview::MinPlaybackSpeed)
					.MaxValue(ArcRecoil::Preview::MaxPlaybackSpeed)
					.MinSliderValue(ArcRecoil::Preview::MinPlaybackSpeed)
					.MaxSliderValue(ArcRecoil::Preview::MaxPlaybackSpeed)
					.Delta(0.05f)
					.Value(this, &SArcRecoilPatternPanel::GetPlaybackSpeed)
					.OnValueChanged(this, &SArcRecoilPatternPanel::HandlePlaybackSpeedChanged)
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcRecoilPatternPanel::GetStatusText)
		]
	];
}

void SArcRecoilPatternPanel::RefreshFromResult(TSharedPtr<const ArcRecoil::Preview::FArcRecoilPreviewResult> InResult)
{
	Result = InResult;

	const int32 LastIndex = GetLastSampleIndex();

	if (ScrubSlider.IsValid())
	{
		ScrubSlider->SetMinAndMaxValues(0.0f, static_cast<float>(LastIndex));
	}

	if (CameraCanvas.IsValid())
	{
		CameraCanvas->RefreshFromResult(Result);
	}

	if (ImpactCanvas.IsValid())
	{
		ImpactCanvas->RefreshFromResult(Result);
	}

	if (!HasPlayableResult())
	{
		bPlaying = false;
		bResumePlaybackAfterScrub = false;
	}

	ApplyCurrentSample(CurrentSample);
}

void SArcRecoilPatternPanel::RepaintCanvases()
{
	if (CameraCanvas.IsValid())
	{
		CameraCanvas->Invalidate(EInvalidateWidgetReason::Paint);
	}

	if (ImpactCanvas.IsValid())
	{
		ImpactCanvas->Invalidate(EInvalidateWidgetReason::Paint);
	}
}

FReply SArcRecoilPatternPanel::HandleForwardPlay()
{
	if (bPlaying)
	{
		bPlaying = false;
		return FReply::Handled();
	}

	if (!HasPlayableResult())
	{
		return FReply::Handled();
	}

	if (CurrentSample >= static_cast<float>(GetLastSampleIndex()))
	{
		ApplyCurrentSample(0.0f);
	}

	bPlaying = true;
	return FReply::Handled();
}

FReply SArcRecoilPatternPanel::HandleForwardStep()
{
	bPlaying = false;

	const ArcRecoil::Preview::FArcRecoilPreviewResult* Data = Result.Get();
	if (Data == nullptr)
	{
		return FReply::Handled();
	}

	const int32 Sample = ArcRecoilPatternPanelPrivate::SampleToIndex(CurrentSample, GetLastSampleIndex());
	const int32 NextEntry = Algo::UpperBound(Data->ShotSampleIndices, Sample);
	if (Data->ShotSampleIndices.IsValidIndex(NextEntry))
	{
		ApplyCurrentSample(static_cast<float>(Data->ShotSampleIndices[NextEntry]));
	}

	return FReply::Handled();
}

FReply SArcRecoilPatternPanel::HandleBackwardStep()
{
	bPlaying = false;

	const ArcRecoil::Preview::FArcRecoilPreviewResult* Data = Result.Get();
	if (Data == nullptr)
	{
		return FReply::Handled();
	}

	const int32 Sample = ArcRecoilPatternPanelPrivate::SampleToIndex(CurrentSample, GetLastSampleIndex());
	const int32 CurrentEntry = Algo::UpperBound(Data->ShotSampleIndices, Sample) - 1;
	if (!Data->ShotSampleIndices.IsValidIndex(CurrentEntry))
	{
		return FReply::Handled();
	}

	const int32 TargetEntry = Sample > Data->ShotSampleIndices[CurrentEntry] ? CurrentEntry : CurrentEntry - 1;
	ApplyCurrentSample(Data->ShotSampleIndices.IsValidIndex(TargetEntry)
		? static_cast<float>(Data->ShotSampleIndices[TargetEntry])
		: 0.0f);

	return FReply::Handled();
}

FReply SArcRecoilPatternPanel::HandleToggleLooping()
{
	UArcRecoilPreviewSettings* SettingsPtr = Settings.Get();
	if (SettingsPtr != nullptr)
	{
		SettingsPtr->bLoop = !SettingsPtr->bLoop;
	}

	return FReply::Handled();
}

bool SArcRecoilPatternPanel::HandleGetLooping() const
{
	const UArcRecoilPreviewSettings* SettingsPtr = Settings.Get();
	return SettingsPtr != nullptr ? SettingsPtr->bLoop : false;
}

EPlaybackMode::Type SArcRecoilPatternPanel::HandleGetPlaybackMode() const
{
	return bPlaying ? EPlaybackMode::PlayingForward : EPlaybackMode::Stopped;
}

void SArcRecoilPatternPanel::HandleTickPlayback(double InCurrentTime, float InDeltaTime)
{
	if (!bPlaying)
	{
		return;
	}

	const ArcRecoil::Preview::FArcRecoilPreviewResult* Data = Result.Get();
	if (!HasPlayableResult() || Data == nullptr)
	{
		bPlaying = false;
		return;
	}

	const UArcRecoilPreviewSettings* SettingsPtr = Settings.Get();
	const float Speed = SettingsPtr != nullptr
		? FMath::Clamp(SettingsPtr->PlaybackSpeed, ArcRecoil::Preview::MinPlaybackSpeed, ArcRecoil::Preview::MaxPlaybackSpeed)
		: 1.0f;

	const float LastIndex = static_cast<float>(GetLastSampleIndex());
	float NextSample = CurrentSample + FMath::Max(InDeltaTime, 0.0f) * Speed / Data->SecondsPerSample;
	if (!FMath::IsFinite(NextSample))
	{
		NextSample = LastIndex;
	}

	if (NextSample >= LastIndex)
	{
		const bool bLoop = SettingsPtr != nullptr ? SettingsPtr->bLoop : false;
		if (bLoop)
		{
			NextSample = FMath::Fmod(NextSample, LastIndex);
		}
		else
		{
			NextSample = LastIndex;
			bPlaying = false;
		}
	}

	ApplyCurrentSample(NextSample);
}

float SArcRecoilPatternPanel::GetScrubValue() const
{
	return CurrentSample;
}

float SArcRecoilPatternPanel::GetScrubStepSize() const
{
	const ArcRecoil::Preview::FArcRecoilPreviewResult* Data = Result.Get();
	return Data != nullptr ? static_cast<float>(FMath::Max(Data->StepsPerShot, 1)) : 1.0f;
}

void SArcRecoilPatternPanel::HandleScrubValueChanged(float NewValue)
{
	ApplyCurrentSample(NewValue);
}

void SArcRecoilPatternPanel::HandleScrubCaptureBegin()
{
	bResumePlaybackAfterScrub = bPlaying;
	bPlaying = false;
}

void SArcRecoilPatternPanel::HandleScrubCaptureEnd()
{
	bPlaying = bResumePlaybackAfterScrub && HasPlayableResult();
	bResumePlaybackAfterScrub = false;
}

float SArcRecoilPatternPanel::GetPlaybackSpeed() const
{
	const UArcRecoilPreviewSettings* SettingsPtr = Settings.Get();
	return SettingsPtr != nullptr ? SettingsPtr->PlaybackSpeed : 1.0f;
}

void SArcRecoilPatternPanel::HandlePlaybackSpeedChanged(float NewValue)
{
	UArcRecoilPreviewSettings* SettingsPtr = Settings.Get();
	if (SettingsPtr != nullptr)
	{
		SettingsPtr->PlaybackSpeed = FMath::Clamp(NewValue,
			ArcRecoil::Preview::MinPlaybackSpeed, ArcRecoil::Preview::MaxPlaybackSpeed);
	}
}

FText SArcRecoilPatternPanel::GetPositionText() const
{
	const ArcRecoil::Preview::FArcRecoilPreviewResult* Data = Result.Get();
	if (Data == nullptr || Data->SampleCount <= 0)
	{
		return LOCTEXT("NoSamples", "no samples");
	}

	const int32 LastIndex = Data->SampleCount - 1;
	const int32 Sample = ArcRecoilPatternPanelPrivate::SampleToIndex(CurrentSample, LastIndex);
	const float Seconds = static_cast<float>(Sample) * Data->SecondsPerSample;
	const int32 TailStart = Data->ShotCount * Data->StepsPerShot;

	if (Sample >= TailStart)
	{
		return FText::FromString(FString::Printf(TEXT("tail - sample %d / %d - %.3f s"),
			Sample, LastIndex, Seconds));
	}

	const int32 ShotNumber = Algo::UpperBound(Data->ShotSampleIndices, Sample);
	return FText::FromString(FString::Printf(TEXT("shot %d / %d - sample %d / %d - %.3f s"),
		ShotNumber, Data->ShotCount, Sample, LastIndex, Seconds));
}

FText SArcRecoilPatternPanel::GetStatusText() const
{
	const ArcRecoil::Preview::FArcRecoilPreviewResult* Data = Result.Get();
	if (Data == nullptr || Data->SampleCount <= 0)
	{
		return LOCTEXT("NoSimulation", "nothing simulated - check MaxShots, SeedCount, RoundsPerMinute and SimulationHz");
	}

	FString Status = FString::Printf(TEXT("runs %d - shots %d - substeps/shot %d - samples %d - %.2f ms/sample"),
		Data->RunCount, Data->ShotCount, Data->StepsPerShot, Data->SampleCount, Data->SecondsPerSample * 1000.0f);

	const UArcRecoilPreviewSettings* SettingsPtr = Settings.Get();

	if (Data->bDraft)
	{
		TArray<FString, TInlineAllocator<2>> DraftFields;
		if (Data->bSeedCapBound)
		{
			DraftFields.Add(FString::Printf(TEXT("%d of %d seeds"), Data->RunCount, Data->FullRunCount));
		}

		if (Data->bSpreadCapBound)
		{
			DraftFields.Add(FString::Printf(TEXT("%d of %d spread samples"), Data->MaxSpreadSamples,
				SettingsPtr != nullptr ? SettingsPtr->SpreadSamplesPerShot : Data->MaxSpreadSamples));
		}

		Status += FString::Printf(TEXT("   [draft preview while dragging - %s, release for full fidelity]"),
			*FString::Join(DraftFields, TEXT(", ")));
	}

	const int32 GenuineSeedCount = SettingsPtr != nullptr ? SettingsPtr->SeedCount : Data->FullRunCount;
	TArray<FString, TInlineAllocator<2>> ClampedFields;
	if (GenuineSeedCount != Data->FullRunCount)
	{
		ClampedFields.Add(FString::Printf(TEXT("runs %d"), GenuineSeedCount));
	}

	if (SettingsPtr != nullptr && SettingsPtr->MaxShots != Data->ShotCount)
	{
		ClampedFields.Add(FString::Printf(TEXT("shots %d"), SettingsPtr->MaxShots));
	}

	if (ClampedFields.Num() > 0)
	{
		Status += FString::Printf(TEXT("   [clamped from %s]"), *FString::Join(ClampedFields, TEXT(" - ")));
	}

	return FText::FromString(Status);
}

void SArcRecoilPatternPanel::ApplyCurrentSample(const float InSample)
{
	CurrentSample = FMath::Clamp(FMath::IsFinite(InSample) ? InSample : 0.0f,
		0.0f, static_cast<float>(GetLastSampleIndex()));

	if (CameraCanvas.IsValid())
	{
		CameraCanvas->SetCurrentSample(CurrentSample);
	}

	if (ImpactCanvas.IsValid())
	{
		ImpactCanvas->SetCurrentSample(CurrentSample);
	}
}

int32 SArcRecoilPatternPanel::GetLastSampleIndex() const
{
	const ArcRecoil::Preview::FArcRecoilPreviewResult* Data = Result.Get();
	return Data != nullptr ? FMath::Max(Data->SampleCount - 1, 0) : 0;
}

bool SArcRecoilPatternPanel::HasPlayableResult() const
{
	const ArcRecoil::Preview::FArcRecoilPreviewResult* Data = Result.Get();
	return Data != nullptr && Data->SampleCount > 1 && Data->SecondsPerSample > 0.0f;
}

#undef LOCTEXT_NAMESPACE
