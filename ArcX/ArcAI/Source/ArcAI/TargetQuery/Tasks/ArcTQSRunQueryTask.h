// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "StateTreeAsyncExecutionContext.h"
#include "TargetQuery/ArcTQSTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "TargetQuery/ArcTQSQuerySubsystem.h"
#include "ArcTQSRunQueryTask.generated.h"

class UArcTQSQueryDefinition;
class UArcTQSQuerySubsystem;
class UMassSignalSubsystem;
struct FArcTQSQueryInstance;

USTRUCT()
struct FArcTQSRunQueryTaskInstanceData
{
	GENERATED_BODY()

	// --- Configuration ---

	// Toggle between DataAsset reference and inline definition
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUseInlineDefinition = false;

	// DataAsset-based query definition (shared, reusable)
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "!bUseInlineDefinition"))
	TObjectPtr<UArcTQSQueryDefinition> QueryDefinition;

	/**
	 * Context locations for the generator to run around.
	 * E.g. smart object locations, waypoints, or any set of points.
	 * Generators will produce items around EACH location in this array.
	 *
	 * If empty, the querier's own location is used as the sole context location.
	 * If populated, the querier location is NOT automatically included — add it explicitly if needed.
	 */
	UPROPERTY(EditAnywhere, Category = Input, meta = (Optional))
	TArray<FVector> ContextLocations;
	
	// --- Inline definition (when bUseInlineDefinition = true) ---

	// Optional context provider that produces ContextLocations dynamically
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BaseStruct = "/Script/ArcAI.ArcTQSContextProvider", EditCondition = "bUseInlineDefinition"))
	FInstancedStruct InlineContextProvider;

	// Generator that produces the initial target pool
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BaseStruct = "/Script/ArcAI.ArcTQSGenerator"))
	FInstancedStruct InlineGenerator;

	// Ordered pipeline of filter/score steps
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (BaseStruct = "/Script/ArcAI.ArcTQSStep", EditCondition = "bUseInlineDefinition"))
	TArray<FInstancedStruct> InlineSteps;

	// How to select from the final scored pool
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bUseInlineDefinition"))
	EArcTQSSelectionMode InlineSelectionMode = EArcTQSSelectionMode::HighestScore;

	// For TopN mode
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bUseInlineDefinition && InlineSelectionMode == EArcTQSSelectionMode::TopN", ClampMin = 1))
	int32 InlineTopN = 5;

	// For AllPassing mode
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (EditCondition = "bUseInlineDefinition && InlineSelectionMode == EArcTQSSelectionMode::AllPassing"))
	float InlineMinPassingScore = 0.0f;

	// For custom top-percent modes: what percentage of the top items to consider
	UPROPERTY(EditAnywhere, Category = Parameter,
		meta = (EditCondition = "bUseInlineDefinition && (InlineSelectionMode == EArcTQSSelectionMode::RandomFromTopPercent || InlineSelectionMode == EArcTQSSelectionMode::WeightedRandomFromTopPercent)",
			ClampMin = 1.0, ClampMax = 100.0))
	float InlineTopPercent = 25.0f;

	// --- Outputs ---

	// Best single result
	UPROPERTY(EditAnywhere, Category = Output)
	FArcTQSTargetItem BestResult;

	// All results (for TopN / AllPassing modes)
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FArcTQSTargetItem> ResultArray;

	// Whether the query found any results
	UPROPERTY(EditAnywhere, Category = Output)
	bool bHasResults = false;

	// --- Optional Inputs ---

	// Explicit items to feed to ExplicitList generator
	UPROPERTY(EditAnywhere, Category = Input, meta = (Optional))
	TArray<FArcTQSTargetItem> ExplicitInput;


	// Override search radius at runtime (if generator supports it)
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (Optional))
	float OverrideRadius = -1.0f;

	// --- Runtime state (not exposed to editor) ---

	int32 QueryId = INDEX_NONE;
	FMassEntityHandle EntityHandle;
};

/**
 * State Tree task that runs a TQS query and outputs results.
 * Integrates with UArcTQSQuerySubsystem for time-sliced async execution.
 *
 * Results are delivered via callback — the State Tree is assumed NOT to be ticking
 * by default. On completion, the entity is signaled via UMassSignalSubsystem so the
 * State Tree wakes up and processes the transition.
 *
 * Supports two modes:
 * - DataAsset reference: set bUseInlineDefinition = false, assign QueryDefinition
 * - Inline: set bUseInlineDefinition = true, configure InlineGenerator/InlineSteps/InlineSelection directly
 */
USTRUCT(meta = (DisplayName = "Arc Run Target Query", Category = "Arc|Query"))
struct ARCAI_API FArcTQSRunQueryTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcTQSRunQueryTaskInstanceData;

	FArcTQSRunQueryTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	// Callback that handles query completion — writes results to instance data,
	// finishes the task (if bFinishOnEnd), and signals the entity.
	static void OnQueryCompleted(
		FArcTQSQueryInstance& CompletedQuery,
		FStateTreeWeakExecutionContext WeakContext,
		UMassSignalSubsystem* SignalSubsystem,
		FMassEntityHandle Entity,
		bool bFinishOnEnd,
		float ReQueryInterval);

	// Whether to finish task on query completion
	UPROPERTY(EditAnywhere, Category = "Task")
	bool bFinishOnEnd = true;

	// Re-query interval when bFinishOnEnd is false. -1 = never auto-requery.
	UPROPERTY(EditAnywhere, Category = "Task", meta = (EditCondition = "!bFinishOnEnd"))
	float ReQueryInterval = -1.0f;

#if WITH_EDITOR
	virtual FName GetIconName() const override
	{
		return FName("StateTreeEditorStyle|Node.Find");
	}
	virtual FColor GetIconColor() const override
	{
		return FColor(64, 180, 220);
	}
#endif

private:
	// Build a query context from the execution context and instance data
	static FArcTQSQueryContext BuildQueryContext(
		FMassStateTreeExecutionContext& MassCtx,
		FMassEntityManager& EntityManager,
		UWorld* World,
		const FInstanceDataType& InstanceData);

	// Submit a query using instance data configuration (DataAsset or inline)
	static int32 SubmitQuery(
		UArcTQSQuerySubsystem* TQSSubsystem,
		const FInstanceDataType& InstanceData,
		const FArcTQSQueryContext& QueryContext,
		FArcTQSQueryFinished OnFinished);
};
