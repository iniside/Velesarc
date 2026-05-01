#pragma once
#include "MassStateTreeTypes.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"
#include "StateTreePropertyRef.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"
#include "ArcKnowledgeTypes.h"

#include "ArcMassGetNextPlanStepTask.generated.h"

USTRUCT()
struct FArcMassGetNextPlanStepTaskInstanceData
{
	GENERATED_BODY()

	// Plan steps to iterate through sequentially
	UPROPERTY(VisibleAnywhere, Category = Input, meta = (CanRefToArray))
	TArray<FArcSmartObjectPlanStep> PlanItemsQueue;

	// Output: entity handle of the smart object for the current step
	UPROPERTY(EditAnywhere, Category = Output)
	FArcMassEntityHandleWrapper SmartObjectEntityHandle;

	// Output: candidate slots found for the current step's smart object
	UPROPERTY(VisibleAnywhere, Category = Output)
	FMassSmartObjectCandidateSlots CandidateSlots;

	// Output: world location of the current step's smart object
	UPROPERTY(EditAnywhere, Category = Output)
	FVector StepLocation = FVector::ZeroVector;

	// Output: knowledge handle for knowledge-sourced steps (invalid for SmartObject steps)
	UPROPERTY(EditAnywhere, Category = Output)
	FArcKnowledgeHandle KnowledgeHandle;

	// Dispatched when all plan steps have been consumed
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnPlanFinished;
};

USTRUCT()
struct FArcMassGetNextPlanStepTaskRuntimeInstanceData
{
	GENERATED_BODY()

	// Plan steps to iterate through sequentially
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	int32 CurrentStep = INDEX_NONE;
};
/**
* Iterates through plan steps one at a time. Each entry advances the step index and outputs
* the current step's smart object entity handle, candidate slots, and world location.
* Fires OnPlanFinished and succeeds when all steps have been consumed. Fails if no steps exist.
*/
USTRUCT(meta = (DisplayName = "Arc Mass Get Next Plan Step", Category = "Smart Object Planner"))
struct FArcMassGetNextPlanStepTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassGetNextPlanStepTaskInstanceData;
	using FExecutionRuntimeDataType = FArcMassGetNextPlanStepTaskRuntimeInstanceData;
	
	FArcMassGetNextPlanStepTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual const UStruct* GetExecutionRuntimeDataType() const override { return FExecutionRuntimeDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};