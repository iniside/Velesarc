#pragma once
#include "SmartObjectPlanner/ArcSmartObjectPlanContainer.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanRequestHandle.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"
#include "GameplayTagContainer.h"
#include "StateTreePropertyRef.h"
#include "Tasks/ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcMassMakePlanMultipleGoalsTask.generated.h"

USTRUCT()
struct FArcMassMakePlanMultipleGoalsTaskInstanceData
{
	GENERATED_BODY()

	// Each entry is a separate goal; one plan request is issued per goal tag set
	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<FGameplayTagContainer> GoalRequiredTags;

	// Tags applied to the entity at the start of planning (used for precondition matching)
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer InitialTags;

	// Maximum distance from SearchOrigin to search for smart objects
	UPROPERTY(EditAnywhere, Category = Parameter)
	float SearchRadius = 1000.f;

	// Maximum number of plans per goal the planner should return
	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 MaxPlans = 5;

	// Override search origin. If zero, defaults to the owning entity's location.
	UPROPERTY(EditAnywhere, Category = Input, meta = (Optional))
	FVector SearchOrigin = FVector::ZeroVector;

	// Direct output of the accumulated plan response
	UPROPERTY(EditAnywhere, Category = Output)
	FArcSmartObjectPlanResponse PlanResponseOut;

	// Property ref to write the accumulated plan response into an external binding
	UPROPERTY(EditAnywhere, Category = Output)
	TStateTreePropertyRef<FArcSmartObjectPlanResponse> PlanResponse;

	/** Tracks pending request handles — stored in instance data to avoid dangling references in delegates. */
	TArray<FArcSmartObjectPlanRequestHandle> PendingHandles;
};

/**
* Submits multiple async plan requests to the Smart Object Planner subsystem, one per goal tag set.
* All requests run in parallel and results are accumulated into a single PlanResponse.
* Completes when all goal requests have finished.
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
USTRUCT(meta = (DisplayName = "Arc Mass Make Plan Multiple Goals", Category = "Smart Object Planner"))
struct FArcMassMakePlanMultipleGoalsTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassMakePlanMultipleGoalsTaskInstanceData;

	FArcMassMakePlanMultipleGoalsTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};