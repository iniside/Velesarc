// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IRewindDebuggerExtension.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"

struct FArcUtilityTraceRequestRecord;

/** Recording extension — toggles ArcUtilityDebugChannel when Rewind Debugger recording starts/stops. */
class FArcUtilityRewindDebuggerRecordingExtension final : public IRewindDebuggerRuntimeExtension
{
	virtual void RecordingStarted() override;
	virtual void RecordingStopped() override;
};

/** Playback extension — draws Utility AI debug shapes in viewport during Rewind scrub. */
class FArcUtilityRewindDebuggerPlaybackExtension : public IRewindDebuggerExtension
{
public:
	virtual FString GetName() override { return TEXT("ArcUtility Playback"); }
	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;
	virtual void Clear(IRewindDebugger* RewindDebugger) override;

private:
	void DrawRequestInViewport(const FArcUtilityTraceRequestRecord& Request, UWorld* World);
	void ClearViewportDraw(UWorld* World);

	double LastScrubTime = -1.0;
};
