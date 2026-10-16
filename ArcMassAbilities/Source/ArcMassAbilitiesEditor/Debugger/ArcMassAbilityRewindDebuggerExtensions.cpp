// Copyright Lukasz Baran. All Rights Reserved.

#include "Debugger/ArcMassAbilityRewindDebuggerExtensions.h"
#include "Debugger/ArcMassAbilityTraceProvider.h"
#include "Debugger/ArcMassAbilityTraceTypes.h"
#include "IRewindDebugger.h"
#include "Debugger/ArcMassAbilityTrace.h"
#include "Trace/Trace.inl"

DEFINE_LOG_CATEGORY(LogArcMABRecording);
DEFINE_LOG_CATEGORY(LogArcMABPlayback);

// ---------------------------------------------------------------------------
// Recording Extension — channel toggle
// ---------------------------------------------------------------------------

void FArcMassAbilityRewindDebuggerRecordingExtension::RecordingStarted()
{
	UE_LOGF(LogArcMABRecording, Log,"[ArcMAB Recording] RecordingStarted — channel was %ls",
		UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel) ? TEXT("ENABLED") : TEXT("DISABLED"));

	if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel))
	{
		UE::Trace::ToggleChannel(TEXT("ArcMassAbilityChannel"), true);
	}

	UE_LOGF(LogArcMABRecording, Log,"[ArcMAB Recording] After toggle: channel is %ls",
		UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel) ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void FArcMassAbilityRewindDebuggerRecordingExtension::RecordingStopped()
{
	UE_LOGF(LogArcMABRecording, Log,"[ArcMAB Recording] RecordingStopped — disabling channel");
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcMassAbilityChannel))
	{
		UE::Trace::ToggleChannel(TEXT("ArcMassAbilityChannel"), false);
	}
}

// ---------------------------------------------------------------------------
// Playback Extension — v1: log active entities, no viewport draw
// ---------------------------------------------------------------------------

void FArcMassAbilityRewindDebuggerPlaybackExtension::Update(
	float DeltaTime, IRewindDebugger* RewindDebugger)
{
	if (!RewindDebugger || !RewindDebugger->GetAnalysisSession())
	{
		return;
	}

	// Use CurrentTraceTime() — provider timestamps are in trace/profile time.
	const double TraceTime = RewindDebugger->CurrentTraceTime();
	if (FMath::IsNearlyEqual(TraceTime, LastTraceTime))
	{
		return;
	}
	LastTraceTime = TraceTime;

	const FArcMassAbilityTraceProvider* Provider =
		RewindDebugger->GetAnalysisSession()->ReadProvider<FArcMassAbilityTraceProvider>(
			FArcMassAbilityTraceProvider::ProviderName);
	if (!Provider)
	{
		return;
	}

	// v1: enumerate entities that have any event in the ±0.3s window and log them.
	// Real viewport drawing deferred — entity world positions are not in the
	// trace payload, so DrawDebug requires additional position capture work.
	const FArcMABEntityRecord* Record = Provider->GetRecordsAtTime(TraceTime, 0.3);
	if (Record)
	{
		UE_LOGF(LogArcMABPlayback, Verbose,"[ArcMAB Playback] TraceTime=%.4f  ActiveEntity=[%u:%u]  "
			     "AbilityActivated=%d EffectApplied=%d AttributeChanged=%d SignalRaised=%d",
			TraceTime,
			Record->EntityIndex,
			Record->EntitySerial,
			Record->AbilityActivated.Num(),
			Record->EffectApplied.Num(),
			Record->AttributeChanged.Num(),
			Record->SignalRaised.Num());
	}
}

void FArcMassAbilityRewindDebuggerPlaybackExtension::Clear(IRewindDebugger* RewindDebugger)
{
	LastTraceTime = -1.0;
}
