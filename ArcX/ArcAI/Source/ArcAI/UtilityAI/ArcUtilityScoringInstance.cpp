// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/ArcUtilityScoringInstance.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcAILogs.h"
#include "HAL/PlatformTime.h"
#include "VisualLogger/VisualLogger.h"
#include "GameFramework/Actor.h"
#include "UtilityAI/Debugger/ArcUtilityTrace.h"

#if ENABLE_VISUAL_LOG
void FArcUtilityScoringInstance::FlushDebugLog()
{
	if (DebugLog.IsEmpty())
	{
		return;
	}

	if (const AActor* LogOwner = Context.QuerierActor.Get())
	{
		UE_VLOG(LogOwner, LogArcUtility, Log, TEXT("%s"), *DebugLog);
	}

	DebugLog.Reset();
}
#endif

#if WITH_ARCUTILITY_TRACE
static FString GetStructDisplayName(const FInstancedStruct& Struct)
{
	if (const UScriptStruct* ScriptStruct = Struct.GetScriptStruct())
	{
		const FString DisplayName = ScriptStruct->GetMetaData(TEXT("DisplayName"));
		return DisplayName.IsEmpty() ? ScriptStruct->GetName() : DisplayName;
	}
	return TEXT("Unknown");
}
#endif

bool FArcUtilityScoringInstance::ExecuteStep(double Deadline)
{
	const double StepStart = FPlatformTime::Seconds();

	if (Status == EArcUtilityScoringStatus::Aborted)
	{
		return true;
	}

	// Phase 1: Start processing
	if (Status == EArcUtilityScoringStatus::Pending)
	{
		if (Entries.Num() == 0 || Targets.Num() == 0)
		{
			Status = EArcUtilityScoringStatus::Failed;
			TotalExecutionTime += FPlatformTime::Seconds() - StepStart;
			return true;
		}

		ScoredPairs.Reserve(Entries.Num() * Targets.Num());
		Status = EArcUtilityScoringStatus::Processing;

#if ENABLE_VISUAL_LOG
		DebugLog += FString::Printf(TEXT("Utility Scoring %d: %d entries x %d targets\n"),
			RequestId, Entries.Num(), Targets.Num());
#endif

#if WITH_ARCUTILITY_TRACE
		bTraceActive = UE_TRACE_CHANNELEXPR_IS_ENABLED(ArcUtilityDebugChannel);
		if (bTraceActive)
		{
			ConsiderationSnapshots.Reserve(Entries.Num() * Targets.Num() * 4);
		}
#endif
	}

	// Phase 2: Score entries x targets
	if (Status == EArcUtilityScoringStatus::Processing)
	{
		if (!RunScoring(Deadline))
		{
			TotalExecutionTime += FPlatformTime::Seconds() - StepStart;
			return false; // Yielded
		}
		Status = EArcUtilityScoringStatus::Selecting;
	}

	// Phase 3: Selection
	if (Status == EArcUtilityScoringStatus::Selecting)
	{
		RunSelection();
		Status = EArcUtilityScoringStatus::Completed;
		TotalExecutionTime += FPlatformTime::Seconds() - StepStart;

#if ENABLE_VISUAL_LOG
		if (Result.bSuccess)
		{
			DebugLog += FString::Printf(TEXT("Completed in %.3fms — Winner: Entry[%d] Score=%.4f\n"),
				TotalExecutionTime * 1000.0, Result.WinningEntryIndex, Result.Score);
		}
		else
		{
			DebugLog += FString::Printf(TEXT("Completed in %.3fms — No entry passed MinScore (%.2f)\n"),
				TotalExecutionTime * 1000.0, MinScore);
		}
		FlushDebugLog();
#endif
		return true;
	}

	return true;
}

void FArcUtilityScoringInstance::Abort()
{
	Status = EArcUtilityScoringStatus::Aborted;
	ScoredPairs.Empty();

#if ENABLE_VISUAL_LOG
	DebugLog.Reset();
#endif
}

bool FArcUtilityScoringInstance::RunScoring(double Deadline)
{
	int32 ItemsProcessed = 0;

	while (CurrentEntryIndex < Entries.Num())
	{
		const FArcUtilityEntry& Entry = Entries[CurrentEntryIndex];
		const int32 NumConsiderations = Entry.Considerations.Num();

		while (CurrentTargetIndex < Targets.Num())
		{
			// Check deadline every 16 items to amortize syscall cost
			if ((ItemsProcessed & 0xF) == 0 && ItemsProcessed > 0 && FPlatformTime::Seconds() >= Deadline)
			{
				return false; // Yield — resume at current entry/target/consideration
			}

			const FArcUtilityTarget& Target = Targets[CurrentTargetIndex];
			Context.EntryIndex = CurrentEntryIndex;

			// Score this entry x target pair
			float Product = 1.0f;
			bool bEarlyOut = false;

			for (int32 ConsiderationIdx = CurrentConsiderationIndex; ConsiderationIdx < NumConsiderations; ++ConsiderationIdx)
			{
				const FArcUtilityConsideration* Consideration = Entry.Considerations[ConsiderationIdx].GetPtr<FArcUtilityConsideration>();
				if (!Consideration)
				{
					continue;
				}

				const float RawScore = Consideration->Score(Target, Context);
				const float CurveScore = Consideration->ResponseCurve.Evaluate(FMath::Clamp(RawScore, 0.0f, 1.0f));
				const float CompensatedScore = FMath::Pow(FMath::Clamp(CurveScore, 0.0f, 1.0f), Consideration->Weight);
#if WITH_ARCUTILITY_TRACE
				if (bTraceActive)
				{
					ConsiderationSnapshots.Add({CurrentEntryIndex, CurrentTargetIndex, ConsiderationIdx,
						GetStructDisplayName(Entry.Considerations[ConsiderationIdx]),
						RawScore, CurveScore, CompensatedScore});
				}
#endif
				Product *= CompensatedScore;

				// Early-out: product can only decrease since all values are [0,1]
				if (Product < MinScore)
				{
					bEarlyOut = true;
					break;
				}
			}

			// Reset consideration index for next target
			CurrentConsiderationIndex = 0;

			if (!bEarlyOut && NumConsiderations > 0)
			{
				// Geometric mean compensation: pow(product, 1/N) to prevent collapse
				const float Compensated = FMath::Pow(Product, 1.0f / static_cast<float>(NumConsiderations));
				const float FinalScore = Compensated * Entry.Weight;

				if (FinalScore >= MinScore)
				{
					ScoredPairs.Add({CurrentEntryIndex, CurrentTargetIndex, FinalScore, Entry.LinkedState.StateHandle});

#if ENABLE_VISUAL_LOG
					DebugLog += FString::Printf(TEXT("  Entry[%d] x Target[%d]: Score=%.4f (raw product=%.4f, compensated=%.4f, weight=%.2f)\n"),
						CurrentEntryIndex, CurrentTargetIndex, FinalScore, Product, Compensated, Entry.Weight);
#endif
				}
			}
#if ENABLE_VISUAL_LOG
			else if (bEarlyOut)
			{
				DebugLog += FString::Printf(TEXT("  Entry[%d] x Target[%d]: EARLY OUT (product=%.4f < min=%.2f)\n"),
					CurrentEntryIndex, CurrentTargetIndex, Product, MinScore);
			}
#endif

			++CurrentTargetIndex;
			++ItemsProcessed;
		}

		CurrentTargetIndex = 0;
		++CurrentEntryIndex;
	}

	return true; // All pairs scored
}

void FArcUtilityScoringInstance::RunSelection()
{
	if (ScoredPairs.IsEmpty())
	{
		Result.bSuccess = false;
		return;
	}

	// Sort descending by score
	ScoredPairs.Sort([](const FScoredPair& A, const FScoredPair& B)
	{
		return A.Score > B.Score;
	});

	int32 WinnerIdx = 0;

	switch (SelectionMode)
	{
	case EArcUtilitySelectionMode::HighestScore:
		WinnerIdx = 0;
		break;

	case EArcUtilitySelectionMode::RandomFromTopPercent:
		{
			const float Pct = FMath::Clamp(TopPercent, 1.0f, 100.0f);
			const int32 PoolSize = FMath::Max(1, FMath::CeilToInt(ScoredPairs.Num() * Pct / 100.0f));
			WinnerIdx = FMath::RandRange(0, PoolSize - 1);
		}
		break;

	case EArcUtilitySelectionMode::WeightedRandom:
		{
			float TotalWeight = 0.0f;
			for (const FScoredPair& Pair : ScoredPairs)
			{
				TotalWeight += Pair.Score;
			}

			if (TotalWeight > 0.0f)
			{
				float Random = FMath::FRand() * TotalWeight;
				WinnerIdx = ScoredPairs.Num() - 1; // Fallback for floating-point drift
				for (int32 i = 0; i < ScoredPairs.Num(); ++i)
				{
					Random -= ScoredPairs[i].Score;
					if (Random <= 0.0f)
					{
						WinnerIdx = i;
						break;
					}
				}
			}
		}
		break;
	}

	const FScoredPair& Winner = ScoredPairs[WinnerIdx];
	Result.WinningEntryIndex = Winner.EntryIndex;
	Result.WinningTarget = Targets[Winner.TargetIndex];
	Result.WinningState = Winner.StateHandle;
	Result.Score = Winner.Score;
	Result.bSuccess = true;
}
