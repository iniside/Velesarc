// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeTypes.h"
#include "StateTreeAsyncExecutionContext.h"
#include "ArcTQSTypes.h"
#include "ArcTQSQueryReference.h"
#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSRunQueryTask.generated.h"

class UArcTQSQuerySubsystem;
class UMassSignalSubsystem;
struct FArcTQSQueryInstance;

USTRUCT()
struct FArcTQSRunQueryTaskInstanceData
{
	GENERATED_BODY()

	// --- Configuration ---

	UPROPERTY(EditAnywhere, Category = Parameter)
	FArcTQSQueryReference Query;

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
 *  Typical usage pattern: run in a sibling state, storing results in a parent state's parameters:
 *   - Parent (Has an TQS result parameter)
 *     - Run Target Query (on success, transition to Use Query Result)
 *     - Use Query Result
 *
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
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

private:
	// Build a query context from the execution context and instance data
	static FArcTQSQueryContext BuildQueryContext(
		FMassStateTreeExecutionContext& MassCtx,
		FMassEntityManager& EntityManager,
		UWorld* World,
		const FInstanceDataType& InstanceData);

	static int32 SubmitQuery(
		UArcTQSQuerySubsystem* TQSSubsystem,
		const FInstanceDataType& InstanceData,
		const FArcTQSQueryContext& QueryContext,
		FArcTQSQueryFinished OnFinished);
};
