// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "UtilityAI/ArcUtilityEntry.h"
#include "StructUtils/InstancedStruct.h"

struct FArcUtilityScorer;

/**
 * Runtime state for an in-progress utility scoring request.
 * Manages the Options × Targets matrix, tracks time-slicing resume points,
 * and performs final selection.
 *
 * Owns copies of entries and targets so it does not depend on any UObject lifetime.
 */
struct ARCAI_API FArcUtilityScoringInstance
{
	int32 RequestId = INDEX_NONE;

	// --- Configuration (owned copies) ---

	TArray<FArcUtilityEntry> Entries;
	TArray<FArcUtilityTarget> Targets;
	FArcUtilityContext Context;

	EArcUtilitySelectionMode SelectionMode = EArcUtilitySelectionMode::HighestScore;
	float MinScore = 0.1f;
	float TopPercent = 25.0f;

	// --- Runtime state ---

	EArcUtilityScoringStatus Status = EArcUtilityScoringStatus::Pending;

	// Resume points for time-slicing
	int32 CurrentEntryIndex = 0;
	int32 CurrentTargetIndex = 0;
	int32 CurrentScorerIndex = 0;

	// Accumulated scored pairs: (EntryIndex, TargetIndex, Score, StateHandle)
	struct FScoredPair
	{
		int32 EntryIndex;
		int32 TargetIndex;
		float Score;
		FStateTreeStateHandle StateHandle;
	};
	TArray<FScoredPair> ScoredPairs;

	// Final result
	FArcUtilityResult Result;

	// Completion callback
	TFunction<void(FArcUtilityScoringInstance&)> OnCompleted;

	// For signal-based notification
	FMassEntityHandle RequestingEntity;

	int32 Priority = 0;
	double StartTime = 0.0;
	double TotalExecutionTime = 0.0;

#if ENABLE_VISUAL_LOG
	FString DebugLog;
#endif

	/**
	 * Execute one incremental chunk of work within the given deadline.
	 * @return true if scoring is now complete (Completed, Failed, or Aborted).
	 */
	bool ExecuteStep(double Deadline);

	void Abort();

private:
	bool RunScoring(double Deadline);
	void RunSelection();

#if ENABLE_VISUAL_LOG
	void FlushDebugLog();
#endif
};
