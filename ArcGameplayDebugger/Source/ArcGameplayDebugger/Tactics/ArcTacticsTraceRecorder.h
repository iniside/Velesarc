// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tactics/ArcTacticsTraceLoader.h"
#include "Templates/UniquePtr.h"

struct FArcTacticsRecording;

enum class EArcTacticsTraceState : uint8
{
	Unavailable,
	Idle,
	Recording,
	Stopping,
	Loading
};

class FArcTacticsTraceRecorder
{
public:
	FArcTacticsTraceRecorder();
	~FArcTacticsTraceRecorder();

	FArcTacticsTraceRecorder(const FArcTacticsTraceRecorder&) = delete;
	FArcTacticsTraceRecorder& operator=(const FArcTacticsTraceRecorder&) = delete;

	void Initialize();
	void Uninitialize();
	void Tick();

	bool StartRecording(float DurationSeconds);
	void StopRecording();
	bool LoadFile(const FString& FilePath);
	void RefreshFiles();

	EArcTacticsTraceState GetState() const { return State; }
	const FString& GetStatusText() const { return StatusText; }
	bool HasError() const { return bStatusIsError; }
	const TArray<FString>& GetFiles() const { return Files; }
	const FString& GetActiveFilePath() const { return ActiveFilePath; }
	float GetRequestedDuration() const { return RequestedDuration; }
	double GetRecordingElapsed() const;
	const FArcTacticsRecording* GetRecording() const { return Recording.Get(); }

	static ARCGAMEPLAYDEBUGGER_API FString GetTraceDirectory();
	static ARCGAMEPLAYDEBUGGER_API TArray<FString> FindTraceFiles();
	static ARCGAMEPLAYDEBUGGER_API FString FindNewestTraceFile();

private:
	void SetStatus(const FString& Text, bool bIsError);
	void RequestStop();
	void RestoreChannel();
	void StopOwnedTraceBlocking();
	void BeginLoad(const FString& FilePath);

	EArcTacticsTraceState State = EArcTacticsTraceState::Idle;
	FString StatusText;
	bool bStatusIsError = false;
	TArray<FString> Files;
	FString ActiveFilePath;
	float RequestedDuration = 0.0f;
	double RecordingStartSeconds = 0.0;
	bool bEnabledChannel = false;
	bool bStopPending = false;
	bool bStoppedExternally = false;
	FArcTacticsTraceLoader Loader;
	TUniquePtr<FArcTacticsRecording> Recording;
};
