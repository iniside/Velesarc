#pragma once
#include "SmartObjectPlanner/ArcSmartObjectPlanContainer.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"
#include "GameplayTagContainer.h"
#include "MassStateTreeTypes.h"
#include "StateTreePropertyRef.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcMassMakeSmartObjectPlanTask.generated.h"

USTRUCT()
struct FArcMassMakeSmartObjectPlanTaskInstanceData
{
	GENERATED_BODY()

	// Tags that the goal smart object must match
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer RequiredTags;

	// Tags applied to the entity at the start of planning (used for precondition matching)
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer InitialTags;

	// Additional conditions evaluated during planning (e.g. resource checks, distance filters)
	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<TInstancedStruct<FArcSmartObjectPlanConditionEvaluator>> CustomConditions;

	// Maximum distance from SearchOrigin to search for smart objects
	UPROPERTY(EditAnywhere, Category = Parameter)
	float SearchRadius = 1000.f;

	// Maximum number of plans the planner should return
	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 MaxPlans = 5;

	// Override search origin. If zero, defaults to the owning entity's location.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (Optional))
	FVector SearchOrigin = FVector::ZeroVector;

	// Resulting plans produced by the planner
	UPROPERTY(EditAnywhere, Category = Output)
	FArcSmartObjectPlanResponse PlanResponse;
};

/**
* Submits an async plan request to the Smart Object Planner subsystem.
* Searches for smart objects matching RequiredTags within SearchRadius of the entity.
* Defaults to the owning entity's location if SearchOrigin is not explicitly set.
* Completes asynchronously when the planner produces results.
*
* Typical StateTree composition for Smart Object planning:
*
* Root (parameters: PlanResponse, PlanSteps[], CurrentStepIdx, SOEntityHandle, CandidateSlots)
* |
* |-- State: Make Plan [MakeSmartObjectPlanTask or MakePlanMultipleGoalsTask]
* |     Async — searches for smart objects, produces PlanResponse
* |     -> Succeeded: go to Select Plan
* |
* |-- State: Select Plan [SelectSmartObjectPlanTask]
* |     Picks one plan from PlanResponse, outputs PlanSteps[]
* |     Only re-entered after current plan finishes to avoid re-rolling mid-execution
* |     -> Succeeded: go to Execute Plan
* |
* '-- State: Execute Plan
*       |
*       |-- State: Get Next Step [GetNextPlanStepTask]
*       |     Advances step index, outputs SOEntityHandle + CandidateSlots for current step
*       |     -> Succeeded: go to Execute Step
*       |     -> OnPlanFinished: all steps consumed, plan complete
*       |
*       '-- State: Execute Step
*             Claim the smart object, move to it, then use it
*             Task: [ClaimSmartObjectTask] -> [Move to SO] -> [UseSmartObjectTask]
*             -> Completed: go back to Get Next Step
*/
USTRUCT(meta = (DisplayName = "Arc Mass Make Smart Object Plan", Category = "Smart Object Planner"))
struct FArcMassMakeSmartObjectPlanTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassMakeSmartObjectPlanTaskInstanceData;

	FArcMassMakeSmartObjectPlanTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};