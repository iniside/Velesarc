// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "StructUtils/InstancedStruct.h"

/**
 * Runtime state for an in-progress TQS query.
 * Manages the target pool, tracks time-slicing resume points, and performs final selection.
 *
 * Owns its own copies of the generator, steps, and selection config so it does not
 * depend on any UObject lifetime (important for State Tree tasks that cannot hold
 * instanced UObjects).
 */
struct ARCAI_API FArcTQSQueryInstance
{
	// Unique handle for this query instance
	int32 QueryId = INDEX_NONE;

	// --- Query configuration (owned copies) ---

	// Optional context provider (must be FArcTQSContextProvider-derived, or empty)
	FInstancedStruct ContextProvider;

	// Generator instanced struct (must be FArcTQSGenerator-derived)
	FInstancedStruct Generator;

	// Ordered pipeline of filter/score steps
	TArray<FInstancedStruct> Steps;

	// Selection config
	EArcTQSSelectionMode SelectionMode = EArcTQSSelectionMode::HighestScore;
	int32 TopN = 5;
	float MinPassingScore = 0.0f;
	float TopPercent = 25.0f;

	// --- Runtime state ---

	// Context snapshot (captured at query creation)
	FArcTQSQueryContext QueryContext;

	// The target pool (populated by generator, modified by steps)
	TArray<FArcTQSTargetItem> Items;

	// Current execution state
	EArcTQSQueryStatus Status = EArcTQSQueryStatus::Pending;

	// Time slicing resume state
	int32 CurrentStepIndex = 0;
	int32 CurrentItemIndex = 0;

	// Final results (populated after selection)
	TArray<FArcTQSTargetItem> Results;

	// Completion callback
	TFunction<void(FArcTQSQueryInstance&)> OnCompleted;

	// For signal-based notification
	FMassEntityHandle RequestingEntity;

	// Priority (higher = processed first within frame budget)
	int32 Priority = 0;

	// Timing
	double StartTime = 0.0;
	double TotalExecutionTime = 0.0;

#if ENABLE_VISUAL_LOG
	// Accumulated log text — built up during execution, flushed as a single
	// UE_VLOG call when the query completes (or fails).
	FString DebugLog;
#endif

	/**
	 * Execute one incremental chunk of work within the given deadline.
	 * @return true if the query is now complete (Completed, Failed, or Aborted).
	 */
	bool ExecuteStep(double Deadline);

	// Abort the query
	void Abort();

private:
	void RunGenerator();
	bool RunProcessingSteps(double Deadline);
	void RunSelection();

#if ENABLE_VISUAL_LOG
	void FlushDebugLog();
#endif
};
