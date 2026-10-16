// Copyright Lukasz Baran. All Rights Reserved.

#include "Tactics/ArcTacticsTraceRecorder.h"

#include "ArcTraceFileName.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Tactics/ArcTacticsRecording.h"

#if WITH_ARC_TACTICS_TRACE_ANALYSIS
#include "Debugger/ArcTacticsTrace.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/TraceAuxiliary.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTacticsTraceRecorder, Log, All);
#endif

namespace ArcTacticsTraceRecorderPrivate
{
#if WITH_ARC_TACTICS_TRACE_ANALYSIS
	const TCHAR* ChannelName = TEXT("ArcTacticsChannel");

	bool IsOwnedTraceActive(const FString& ActiveFilePath)
	{
		return FTraceAuxiliary::IsConnected() && FTraceAuxiliary::GetTraceDestinationString() == ActiveFilePath;
	}

	bool IsOwnedTraceClosed(const FString& ActiveFilePath)
	{
		if (!FTraceAuxiliary::IsConnected())
		{
			return true;
		}
		const FString Destination = FTraceAuxiliary::GetTraceDestinationString();
		return !Destination.IsEmpty() && Destination != ActiveFilePath;
	}

	void EmitRecordingStarted()
	{
		if (!GEngine)
		{
			return;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			const UWorld* World = Context.World();
			if (World && World->IsGameWorld())
			{
				TRACE_ARCTACTICS_RECORDING_STARTED(World);
			}
		}
	}
#else
	const TCHAR* UnavailableText = TEXT("Trace analysis unavailable in this configuration");
#endif
}

FArcTacticsTraceRecorder::FArcTacticsTraceRecorder()
{
#if !WITH_ARC_TACTICS_TRACE_ANALYSIS
	State = EArcTacticsTraceState::Unavailable;
	StatusText = ArcTacticsTraceRecorderPrivate::UnavailableText;
#endif
}

FArcTacticsTraceRecorder::~FArcTacticsTraceRecorder()
{
	Uninitialize();
}

void FArcTacticsTraceRecorder::Initialize()
{
	RefreshFiles();
}

void FArcTacticsTraceRecorder::Uninitialize()
{
#if WITH_ARC_TACTICS_TRACE_ANALYSIS
	if (State == EArcTacticsTraceState::Recording || (State == EArcTacticsTraceState::Stopping && bStopPending))
	{
		StopOwnedTraceBlocking();
	}
	RestoreChannel();
	Loader.Cancel();
	Recording.Reset();
	State = EArcTacticsTraceState::Idle;
	bStopPending = false;
	bStoppedExternally = false;
	ActiveFilePath.Reset();
	StatusText.Reset();
	bStatusIsError = false;
#endif
}

void FArcTacticsTraceRecorder::Tick()
{
#if WITH_ARC_TACTICS_TRACE_ANALYSIS
	switch (State)
	{
	case EArcTacticsTraceState::Recording:
		if (!ArcTacticsTraceRecorderPrivate::IsOwnedTraceActive(ActiveFilePath))
		{
			bStoppedExternally = true;
			bStopPending = false;
			State = EArcTacticsTraceState::Stopping;
			RestoreChannel();
			SetStatus(TEXT("Trace was stopped outside the Tactics recorder"), true);
		}
		else if (RequestedDuration > 0.0f && GetRecordingElapsed() >= RequestedDuration)
		{
			RequestStop();
		}
		break;
	case EArcTacticsTraceState::Stopping:
		if (bStopPending)
		{
			if (!ArcTacticsTraceRecorderPrivate::IsOwnedTraceActive(ActiveFilePath) || FTraceAuxiliary::Stop())
			{
				bStopPending = false;
				RestoreChannel();
			}
		}
		if (!bStopPending && ArcTacticsTraceRecorderPrivate::IsOwnedTraceClosed(ActiveFilePath))
		{
			BeginLoad(ActiveFilePath);
		}
		break;
	case EArcTacticsTraceState::Loading:
		if (Loader.IsFinished())
		{
			Recording = Loader.TakeResult();
			State = EArcTacticsTraceState::Idle;
			RefreshFiles();
			if (Recording.IsValid())
			{
				SetStatus(FString::Printf(TEXT("Loaded %d entities, %.1f s from %s%s"),
					Recording->Entities.Num(),
					Recording->Duration,
					*FPaths::GetCleanFilename(Recording->FilePath),
					bStoppedExternally ? TEXT(" (stopped externally)") : TEXT("")), false);
			}
			else
			{
				SetStatus(TEXT("Analysis produced no recording"), true);
			}
			bStoppedExternally = false;
		}
		break;
	default:
		break;
	}
#endif
}

bool FArcTacticsTraceRecorder::StartRecording(const float DurationSeconds)
{
#if WITH_ARC_TACTICS_TRACE_ANALYSIS
	if (State != EArcTacticsTraceState::Idle)
	{
		SetStatus(TEXT("Recorder is busy"), true);
		return false;
	}
	if (FTraceAuxiliary::IsConnected())
	{
		SetStatus(FString::Printf(
			TEXT("Refused: a trace is already running (%s). Stop Insights, -trace or the Trace panel first."),
			*FTraceAuxiliary::GetTraceDestinationString()), true);
		return false;
	}

	const bool bChannelWasEnabled = TRACE_ARCTACTICS_ENABLED();
	const FString RequestedPath = FPaths::Combine(GetTraceDirectory(), ArcTraceFileName::MakeTraceFileName(TEXT("-Tactics")));
	FTraceAuxiliary::FOptions Options;
	Options.bTruncateFile = true;
	Options.bExcludeTail = true;
	if (!FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *RequestedPath, ArcTacticsTraceRecorderPrivate::ChannelName, &Options))
	{
		if (!bChannelWasEnabled && TRACE_ARCTACTICS_ENABLED())
		{
			FTraceAuxiliary::DisableChannels(ArcTacticsTraceRecorderPrivate::ChannelName);
		}
		SetStatus(FString::Printf(TEXT("Failed to start trace to %s"), *RequestedPath), true);
		return false;
	}

	bEnabledChannel = !bChannelWasEnabled;
	ActiveFilePath = FTraceAuxiliary::GetTraceDestinationString();
	RequestedDuration = DurationSeconds;
	RecordingStartSeconds = FPlatformTime::Seconds();
	bStopPending = false;
	bStoppedExternally = false;
	Loader.Cancel();
	Recording.Reset();
	State = EArcTacticsTraceState::Recording;
	ArcTacticsTraceRecorderPrivate::EmitRecordingStarted();

	if (TRACE_ARCTACTICS_ENABLED())
	{
		SetStatus(FString::Printf(TEXT("Recording to %s"), *ActiveFilePath), false);
	}
	else
	{
		SetStatus(FString::Printf(TEXT("Recording to %s, but ArcTacticsChannel could not be enabled"), *ActiveFilePath), true);
	}
	return true;
#else
	return false;
#endif
}

void FArcTacticsTraceRecorder::StopRecording()
{
	if (State == EArcTacticsTraceState::Recording)
	{
		RequestStop();
	}
}

bool FArcTacticsTraceRecorder::LoadFile(const FString& FilePath)
{
#if WITH_ARC_TACTICS_TRACE_ANALYSIS
	if (State != EArcTacticsTraceState::Idle)
	{
		SetStatus(TEXT("Recorder is busy"), true);
		return false;
	}
	bStoppedExternally = false;
	BeginLoad(FilePath);
	return State == EArcTacticsTraceState::Loading;
#else
	return false;
#endif
}

void FArcTacticsTraceRecorder::RefreshFiles()
{
	Files = FindTraceFiles();
}

TArray<FString> FArcTacticsTraceRecorder::FindTraceFiles()
{
	const FString Directory = GetTraceDirectory();
	TArray<FString> FileNames;
	IFileManager::Get().FindFiles(FileNames, *Directory, TEXT(".utrace"));
	FileNames.Sort([](const FString& Lhs, const FString& Rhs)
	{
		return Lhs > Rhs;
	});
	TArray<FString> Paths;
	Paths.Reserve(FileNames.Num());
	for (const FString& FileName : FileNames)
	{
		Paths.Add(FPaths::Combine(Directory, FileName));
	}
	return Paths;
}

FString FArcTacticsTraceRecorder::FindNewestTraceFile()
{
	const TArray<FString> Paths = FindTraceFiles();
	return Paths.IsEmpty() ? FString() : Paths[0];
}

double FArcTacticsTraceRecorder::GetRecordingElapsed() const
{
	return State == EArcTacticsTraceState::Recording ? FPlatformTime::Seconds() - RecordingStartSeconds : 0.0;
}

FString FArcTacticsTraceRecorder::GetTraceDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ArcTactics"), TEXT("Traces")));
}

void FArcTacticsTraceRecorder::SetStatus(const FString& Text, const bool bIsError)
{
	StatusText = Text;
	bStatusIsError = bIsError;
}

void FArcTacticsTraceRecorder::RequestStop()
{
#if WITH_ARC_TACTICS_TRACE_ANALYSIS
	State = EArcTacticsTraceState::Stopping;
	bStopPending = true;
	SetStatus(TEXT("Stopping trace"), false);
#endif
}

void FArcTacticsTraceRecorder::RestoreChannel()
{
#if WITH_ARC_TACTICS_TRACE_ANALYSIS
	if (bEnabledChannel)
	{
		FTraceAuxiliary::DisableChannels(ArcTacticsTraceRecorderPrivate::ChannelName);
		bEnabledChannel = false;
	}
#endif
}

void FArcTacticsTraceRecorder::StopOwnedTraceBlocking()
{
#if WITH_ARC_TACTICS_TRACE_ANALYSIS
	const double Deadline = FPlatformTime::Seconds() + 1.0;
	while (ArcTacticsTraceRecorderPrivate::IsOwnedTraceActive(ActiveFilePath))
	{
		if (FTraceAuxiliary::Stop())
		{
			return;
		}
		if (FPlatformTime::Seconds() >= Deadline)
		{
			UE_LOGF(LogArcTacticsTraceRecorder, Warning, "Could not stop the Tactics trace %ls during teardown", *ActiveFilePath);
			return;
		}
		FPlatformProcess::Sleep(0.01f);
	}
#endif
}

void FArcTacticsTraceRecorder::BeginLoad(const FString& FilePath)
{
	Recording.Reset();
	FString Error;
	if (!Loader.Start(FilePath, Error))
	{
		State = EArcTacticsTraceState::Idle;
		SetStatus(Error, true);
		return;
	}
	State = EArcTacticsTraceState::Loading;
	SetStatus(FString::Printf(TEXT("Loading %s"), *FPaths::GetCleanFilename(FilePath)), false);
}
