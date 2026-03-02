// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSRewindDebuggerExtensions.h"
#include "ArcTQSTraceProvider.h"
#include "ArcTQSTraceTypes.h"
#include "IRewindDebugger.h"
#include "IGameplayProvider.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TargetQuery/Debugger/ArcTQSTrace.h" // for ArcTQSDebugChannel
#include "Trace/Trace.inl"

DEFINE_LOG_CATEGORY_STATIC(LogArcTQSPlayback, Log, All);

// --- Recording Extension (channel toggle) ---

void FArcTQSRewindDebuggerRecordingExtension::RecordingStarted()
{
	UE_LOG(LogArcTQSPlayback, Log, TEXT("[TQS Recording] RecordingStarted — Channel was %s"),
		UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTQSDebugChannel) ? TEXT("ENABLED") : TEXT("DISABLED"));
	if (!UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTQSDebugChannel))
	{
		UE::Trace::ToggleChannel(TEXT("ArcTQSDebugChannel"), true);
	}
	UE_LOG(LogArcTQSPlayback, Log, TEXT("[TQS Recording] After toggle: Channel is %s"),
		UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTQSDebugChannel) ? TEXT("ENABLED") : TEXT("DISABLED"));
}

void FArcTQSRewindDebuggerRecordingExtension::RecordingStopped()
{
	UE_LOG(LogArcTQSPlayback, Log, TEXT("[TQS Recording] RecordingStopped — disabling channel"));
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcTQSDebugChannel))
	{
		UE::Trace::ToggleChannel(TEXT("ArcTQSDebugChannel"), false);
	}
}

// --- Playback Extension (viewport drawing) ---

// Lerp from red (0.0) through yellow (0.5) to green (1.0)
// Matches GameplayDebuggerCategory_ArcTQS::ScoreToColor
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

void FArcTQSRewindDebuggerPlaybackExtension::Update(float DeltaTime, IRewindDebugger* RewindDebugger)
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

	const FArcTQSTraceProvider* Provider = RewindDebugger->GetAnalysisSession()->ReadProvider<FArcTQSTraceProvider>(
		FArcTQSTraceProvider::ProviderName);
	if (!Provider)
	{
		return;
	}

	// Draw all completed queries at this trace time
	TArray<const FArcTQSTraceQueryRecord*> Queries;
	Provider->GetCompletedQueriesAtTime(TraceTime, Queries);

	static bool bLoggedOnce = false;
	if (!bLoggedOnce || Queries.Num() > 0)
	{
		UE_LOG(LogArcTQSPlayback, Log, TEXT("[TQS Playback] TraceTime=%.4f QueriesFound=%d HasData=%d"),
			TraceTime, Queries.Num(), Provider->HasData());
		bLoggedOnce = (Queries.Num() == 0);
	}

	// --- DIAGNOSTIC: Check GameplayProvider for our TRACE_INSTANCE objects ---
	static bool bGameplayProviderChecked = false;
	if (!bGameplayProviderChecked && Provider->HasData())
	{
		bGameplayProviderChecked = true;
		const IGameplayProvider* GameplayProvider = RewindDebugger->GetAnalysisSession()->ReadProvider<IGameplayProvider>(TEXT("GameplayProvider"));
		if (GameplayProvider)
		{
			UE_LOG(LogArcTQSPlayback, Warning, TEXT("[TQS Diag] === GameplayProvider check ==="));

			// Check each InstanceId from our provider
			Provider->EnumerateRecords([GameplayProvider](uint64 InstanceId, const FArcTQSTraceQueryRecord& Record)
			{
				const FObjectInfo* ObjInfo = GameplayProvider->FindObjectInfo(InstanceId);
				if (ObjInfo)
				{
					UE_LOG(LogArcTQSPlayback, Warning, TEXT("[TQS Diag] Instance %llu FOUND in GameplayProvider: Name='%s' OuterId=%llu ClassId=%llu"),
						InstanceId, ObjInfo->Name, ObjInfo->GetOuterUObjectId(), ObjInfo->ClassId);

					// Check if the outer (subsystem) is also present
					const FObjectInfo* OuterInfo = GameplayProvider->FindObjectInfo(ObjInfo->GetOuterId());
					if (OuterInfo)
					{
						UE_LOG(LogArcTQSPlayback, Warning, TEXT("[TQS Diag]   Outer FOUND: Name='%s' OuterId=%llu"),
							OuterInfo->Name, OuterInfo->GetOuterUObjectId());

						// Enumerate sub-objects of the outer to see if our instance is a child
						int32 SubObjectCount = 0;
						bool bFoundSelf = false;
						GameplayProvider->EnumerateSubobjects(ObjInfo->GetOuterId(), [&](const RewindDebugger::FObjectId& SubId)
						{
							++SubObjectCount;
							if (SubId.GetMainId() == InstanceId)
							{
								bFoundSelf = true;
							}
						});
						UE_LOG(LogArcTQSPlayback, Warning, TEXT("[TQS Diag]   Outer has %d sub-objects, self found=%d"),
							SubObjectCount, bFoundSelf);
					}
					else
					{
						UE_LOG(LogArcTQSPlayback, Warning, TEXT("[TQS Diag]   Outer NOT FOUND in GameplayProvider (OuterId=%llu)"),
							ObjInfo->GetOuterUObjectId());
					}

					// Check recording lifetime
					const TRange<double> Lifetime = GameplayProvider->GetObjectRecordingLifetime(InstanceId);
					UE_LOG(LogArcTQSPlayback, Warning, TEXT("[TQS Diag]   Lifetime: [%.4f .. %.4f]"),
						Lifetime.HasLowerBound() ? Lifetime.GetLowerBoundValue() : -1.0,
						Lifetime.HasUpperBound() ? Lifetime.GetUpperBoundValue() : -1.0);
				}
				else
				{
					UE_LOG(LogArcTQSPlayback, Warning, TEXT("[TQS Diag] Instance %llu NOT FOUND in GameplayProvider!"),
						InstanceId);
				}
			});
		}
		else
		{
			UE_LOG(LogArcTQSPlayback, Warning, TEXT("[TQS Diag] GameplayProvider is null!"));
		}
	}

	for (const FArcTQSTraceQueryRecord* Query : Queries)
	{
		DrawQueryInViewport(*Query, World);
	}
}

void FArcTQSRewindDebuggerPlaybackExtension::Clear(IRewindDebugger* RewindDebugger)
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

void FArcTQSRewindDebuggerPlaybackExtension::ClearViewportDraw(UWorld* World)
{
	FlushPersistentDebugLines(World);
	FlushDebugStrings(World);
}

void FArcTQSRewindDebuggerPlaybackExtension::DrawQueryInViewport(
	const FArcTQSTraceQueryRecord& Query, UWorld* World)
{
	const float LifeTime = -1.0f; // Persistent until cleared
	const uint8 DepthPriority = SDPG_World;

	// Build result index set
	TSet<int32> ResultIndexSet;
	for (int32 Idx : Query.ResultIndices)
	{
		ResultIndexSet.Add(Idx);
	}

	// Draw querier
	DrawDebugPoint(World, Query.QuerierLocation + FVector(0, 0, 50), 20.0f, FColor::Blue, true, LifeTime, DepthPriority);
	DrawDebugDirectionalArrow(World, Query.QuerierLocation + FVector(0, 0, 50),
		Query.QuerierLocation + Query.QuerierForward * 200.0f + FVector(0, 0, 50),
		30.0f, FColor::Blue, true, LifeTime, DepthPriority, 2.0f);

	// Draw context locations
	for (const FVector& CtxLoc : Query.ContextLocations)
	{
		if (!CtxLoc.Equals(Query.QuerierLocation, 10.0f))
		{
			DrawDebugSphere(World, CtxLoc, 50.0f, 8, FColor::Cyan, true, LifeTime, DepthPriority, 1.0f);
		}
	}

	// Draw all items
	for (int32 i = 0; i < Query.FinalItems.Num(); ++i)
	{
		const FArcTQSTraceItemRecord& Item = Query.FinalItems[i];
		const bool bIsResult = ResultIndexSet.Contains(i);

		if (!Item.bValid)
		{
			// Filtered: small grey point
			DrawDebugPoint(World, Item.Location + FVector(0, 0, 15), 6.0f,
				FColor(80, 80, 80), true, LifeTime, DepthPriority);
			continue;
		}

		const FColor Color = ScoreToColor(Item.Score);

		if (bIsResult)
		{
			// Winner: large green sphere + score label + line from querier
			DrawDebugSphere(World, Item.Location, 30.0f, 12, FColor::Green, true, LifeTime, DepthPriority, 2.0f);
			DrawDebugString(World, Item.Location + FVector(0, 0, 60),
				FString::Printf(TEXT("%.3f"), Item.Score), nullptr, FColor::Green, LifeTime, true);
			DrawDebugLine(World, Query.QuerierLocation + FVector(0, 0, 40),
				Item.Location + FVector(0, 0, 40), FColor::Green, true, LifeTime, DepthPriority, 1.5f);
		}
		else
		{
			// Valid but not selected: small colored sphere
			DrawDebugPoint(World, Item.Location + FVector(0, 0, 15), 10.0f,
				Color, true, LifeTime, DepthPriority);
		}
	}
}
