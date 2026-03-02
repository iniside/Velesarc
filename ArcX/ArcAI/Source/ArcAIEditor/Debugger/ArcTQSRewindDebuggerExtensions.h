// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "IRewindDebuggerExtension.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"

struct FArcTQSTraceQueryRecord;

/** Recording extension — toggles ArcTQSDebugChannel when Rewind Debugger recording starts/stops. */
class FArcTQSRewindDebuggerRecordingExtension final : public IRewindDebuggerRuntimeExtension
{
	virtual void RecordingStarted() override;
	virtual void RecordingStopped() override;
};

/** Playback extension — draws TQS debug shapes in viewport during Rewind scrub. */
class FArcTQSRewindDebuggerPlaybackExtension : public IRewindDebuggerExtension
{
public:
	virtual FString GetName() override { return TEXT("ArcTQS Playback"); }
	virtual void Update(float DeltaTime, IRewindDebugger* RewindDebugger) override;
	virtual void Clear(IRewindDebugger* RewindDebugger) override;

private:
	void DrawQueryInViewport(const FArcTQSTraceQueryRecord& Query, UWorld* World);
	void ClearViewportDraw(UWorld* World);

	double LastScrubTime = -1.0;
};
