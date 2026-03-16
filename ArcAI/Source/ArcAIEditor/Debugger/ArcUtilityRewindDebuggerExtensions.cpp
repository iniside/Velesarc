// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcUtilityRewindDebuggerExtensions.h"
#include "ArcUtilityTraceProvider.h"
#include "ArcUtilityTraceTypes.h"
#include "IRewindDebugger.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "UtilityAI/Debugger/ArcUtilityTrace.h"
#include "Trace/Trace.inl"

DEFINE_LOG_CATEGORY_STATIC(LogArcUtilityPlayback, Log, All);

// --- Recording Extension (channel toggle) ---

void FArcUtilityRewindDebuggerRecordingExtension::RecordingStarted()
{
	UE_LOG(LogArcUtilityPlayback, Log, TEXT("[Utility Recording] RecordingStarted — Channel was %s"),
		UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcUtilityDebugChannel) ? TEXT("ENABLED") : TEXT("DISABLED"));
	if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcUtilityDebugChannel))
	{
		UE::Trace::ToggleChannel(TEXT("ArcUtilityDebugChannel"), true);
	}
	UE_LOG(LogArcUtilityPlayback, Log, TEXT("[Utility Recording] After toggle: Channel is %s"),
		UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcUtilityDebugChannel) ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void FArcUtilityRewindDebuggerRecordingExtension::RecordingStopped()
{
	UE_LOG(LogArcUtilityPlayback, Log, TEXT("[Utility Recording] RecordingStopped — disabling channel"));
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcUtilityDebugChannel))
	{
		UE::Trace::ToggleChannel(TEXT("ArcUtilityDebugChannel"), false);
	}
}

// --- Playback Extension (viewport drawing) ---

namespace Arcx::Rewind
{
	// Lerp from red (0.0) through yellow (0.5) to green (1.0)
	static FColor ScoreToColor(float Score)
	{
		Score = FMath::Clamp(Score, 0.0f, 1.0f);
		if (Score < 0.5f)
		{
			const float T = Score * 2.0f;
			return FColor(255, static_cast<uint8>(T * 255), 0);
		}
		else
		{
			const float T = (Score - 0.5f) * 2.0f;
			return FColor(static_cast<uint8>((1.0f - T) * 255), 255, 0);
		}
	}	
}


void FArcUtilityRewindDebuggerPlaybackExtension::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
{
	if (!RewindDebugger || !RewindDebugger->GetAnalysisSession())
	{
		return;
	}

	// Use CurrentTraceTime() — our provider timestamps are in profile/trace time
	// (from FEventTime::AsSeconds(Cycle)), NOT recording/elapsed time (GetScrubTime).
	const double TraceTime = RewindDebugger->CurrentTraceTime();
	if (FMath::IsNearlyEqual(TraceTime, LastScrubTime))
	{
		return;
	}
	LastScrubTime = TraceTime;

	UWorld* World = RewindDebugger->GetWorldToVisualize();
	if (!World)
	{
		return;
	}

	ClearViewportDraw(World);

	const FArcUtilityTraceProvider* Provider = RewindDebugger->GetAnalysisSession()->ReadProvider<FArcUtilityTraceProvider>(
		FArcUtilityTraceProvider::ProviderName);
	if (!Provider)
	{
		return;
	}

	// Draw all completed requests at this trace time
	TArray<const FArcUtilityTraceRequestRecord*> Requests;
	Provider->GetCompletedRequestsAtTime(TraceTime, Requests);

	static bool bLoggedOnce = false;
	if (!bLoggedOnce || Requests.Num() > 0)
	{
		UE_LOG(LogArcUtilityPlayback, Log, TEXT("[Utility Playback] TraceTime=%.4f RequestsFound=%d HasData=%d"),
			TraceTime, Requests.Num(), Provider->HasData());
		bLoggedOnce = (Requests.Num() == 0);
	}

	for (const FArcUtilityTraceRequestRecord* Request : Requests)
	{
		DrawRequestInViewport(*Request, World);
	}
}

void FArcUtilityRewindDebuggerPlaybackExtension::Clear(IRewindDebugger* RewindDebugger)
{
	LastScrubTime = -1.0;
	if (RewindDebugger)
	{
		if (UWorld* World = RewindDebugger->GetWorldToVisualize())
		{
			ClearViewportDraw(World);
		}
	}
}

void FArcUtilityRewindDebuggerPlaybackExtension::ClearViewportDraw(UWorld* World)
{
	FlushPersistentDebugLines(World);
	FlushDebugStrings(World);
}

void FArcUtilityRewindDebuggerPlaybackExtension::DrawRequestInViewport(
	const FArcUtilityTraceRequestRecord& Request, UWorld* World)
{
	const float LifeTime = -1.0f; // Persistent until cleared
	const uint8 DepthPriority = SDPG_World;

	// Build set of target indices that appear in ScoredPairs (scored targets)
	TSet<int32> ScoredTargetIndices;
	for (const FArcUtilityTraceScoredPair& Pair : Request.ScoredPairs)
	{
		ScoredTargetIndices.Add(Pair.TargetIndex);
	}

	// Draw querier: blue point + directional arrow
	DrawDebugPoint(World, Request.QuerierLocation + FVector(0, 0, 50), 20.0f, FColor::Blue, true, LifeTime, DepthPriority);
	DrawDebugDirectionalArrow(World, Request.QuerierLocation + FVector(0, 0, 50),
		Request.QuerierLocation + Request.QuerierForward * 200.0f + FVector(0, 0, 50),
		30.0f, FColor::Blue, true, LifeTime, DepthPriority, 2.0f);

	// Draw all targets
	for (int32 i = 0; i < Request.Targets.Num(); ++i)
	{
		const FArcUtilityTraceTargetRecord& Target = Request.Targets[i];

		if (i == Request.WinnerTargetIndex)
		{
			// Winner: large green sphere + score label + green line to querier
			DrawDebugSphere(World, Target.Location, 30.0f, 12, FColor::Green, true, LifeTime, DepthPriority, 2.0f);
			DrawDebugString(World, Target.Location + FVector(0, 0, 60),
				FString::Printf(TEXT("%.3f"), Request.WinnerScore), nullptr, FColor::Green, LifeTime, true);
			DrawDebugLine(World, Request.QuerierLocation + FVector(0, 0, 40),
				Target.Location + FVector(0, 0, 40), FColor::Green, true, LifeTime, DepthPriority, 1.5f);
		}
		else if (ScoredTargetIndices.Contains(i))
		{
			// Scored target: colored point by best score across all entry pairs for this target
			float BestScore = 0.0f;
			for (const FArcUtilityTraceScoredPair& Pair : Request.ScoredPairs)
			{
				if (Pair.TargetIndex == i)
				{
					BestScore = FMath::Max(BestScore, Pair.Score);
				}
			}
			const FColor Color = Arcx::Rewind::ScoreToColor(BestScore);
			DrawDebugPoint(World, Target.Location + FVector(0, 0, 15), 10.0f,
				Color, true, LifeTime, DepthPriority);
		}
		else
		{
			// Not scored: small grey point
			DrawDebugPoint(World, Target.Location + FVector(0, 0, 15), 6.0f,
				FColor(80, 80, 80), true, LifeTime, DepthPriority);
		}
	}
}
