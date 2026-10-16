// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IRewindDebuggerExtension.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArcMABRecording, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogArcMABPlayback,  Log, All);

// ---------------------------------------------------------------------------
// FArcMassAbilityRewindDebuggerRecordingExtension
//
// Toggles ArcMassAbilityChannel when Rewind Debugger recording starts/stops.
// ---------------------------------------------------------------------------

class FArcMassAbilityRewindDebuggerRecordingExtension final : public IRewindDebuggerRuntimeExtension
{
	virtual void RecordingStarted() override;
	virtual void RecordingStopped() override;
};

// ---------------------------------------------------------------------------
// FArcMassAbilityRewindDebuggerPlaybackExtension
//
// v1: logs active entities at the current trace time.
// Real viewport overlay (DrawDebug) deferred to a follow-up — entity positions
// are not in the trace payload and resolving them requires additional work.
// ---------------------------------------------------------------------------

class FArcMassAbilityRewindDebuggerPlaybackExtension : public IRewindDebuggerExtension
{
public:
	virtual FString GetName() override { return TEXT("ArcMassAbility Playback"); }
	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;
	virtual void Clear(IRewindDebugger* RewindDebugger) override;

private:
	double LastTraceTime = -1.0;
};
