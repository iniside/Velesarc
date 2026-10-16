// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSTypes.h"
#include "ArcTQSStep.h"
#include "ArcTQSBindings.h"
#include "ArcTQSQueryResult.h"
#include "PropertyBindingDataView.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"

class UArcTQSQueryDefinition;

/**
 * Runtime state for an in-progress TQS query.
 * Manages the target pool, tracks time-slicing resume points, and performs final selection.
 */
struct ARCTARGETQUERY_API FArcTQSQueryInstance
{
	// Unique handle for this query instance
	int32 QueryId = INDEX_NONE;

	// --- Query configuration (owned copies) ---

	TWeakObjectPtr<const UArcTQSQueryDefinition> Definition;

	uint32 CompiledRevision = 0;

	FInstancedPropertyBag Parameters;

	TArray<FInstancedStruct> Functions;

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

	FArcTQSQuerierInputs QuerierInputs;

	FArcTQSItemInputs ItemInputs;

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

	TArray<int32> ResultIndices;

	FRandomStream RandomStream;

	EArcTQSWorldRequirements UnavailableRequirements = EArcTQSWorldRequirements::None;

	bool bGeneratorLevelOnly = false;

	bool bEmitTrace = true;

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

#if !UE_BUILD_SHIPPING
	TArray<FArcTQSStepResult> StepResults;
	TArray<int32> FilteredByStep;
	double DebugCollectionOverhead = 0.0;
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
	const FArcTQSCompiledData* GetCompiledData() const;
	FPropertyBindingDataView GetSourceView(FArcTQSDataHandle Handle);
	bool ApplyBindings(const FArcTQSCompiledData& CompiledData, FPropertyBindingIndex16 BatchIndex, FPropertyBindingDataView TargetView);
	bool EvaluateFunctions(const FArcTQSCompiledData& CompiledData, const FPropertyBindingCopyInfoBatch& Batch);
	void FillItemInputs(const FArcTQSTargetItem& Item);
	bool RunProcessingSteps(double Deadline);
	void RunSelection();

#if ENABLE_VISUAL_LOG
	void FlushDebugLog();
#endif
};
