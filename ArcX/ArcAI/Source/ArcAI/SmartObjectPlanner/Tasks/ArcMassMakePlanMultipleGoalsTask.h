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

	UPROPERTY(EditAnywhere, Category = Parameter)
	TArray<FGameplayTagContainer> GoalRequiredTags;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer InitialTags;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float SearchRadius = 1000.f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	int32 MaxPlans = 5;

	UPROPERTY(EditAnywhere, Category = Input)
	FVector SearchOrigin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Output)
	FArcSmartObjectPlanResponse PlanResponseOut;

	UPROPERTY(EditAnywhere, Category = Out)
	TStateTreePropertyRef<FArcSmartObjectPlanResponse> PlanResponse;

	/** Tracks pending request handles — stored in instance data to avoid dangling references in delegates. */
	TArray<FArcSmartObjectPlanRequestHandle> PendingHandles;
};

/**
* Task that runs an async environment query and outputs the result to an outside parameter. Supports Actor and vector types EQS.
* The task is usually run in a sibling state to the result user will be with the data being stored in the parent state's parameters.
* - Parent (Has an EQS result parameter)
*	- Run Env Query (If success go to Use Query Result)
*	- Use Query Result
*/
USTRUCT(meta = (DisplayName = "Arc Mass Make Plan Multiple Goals", Category = "Common"))
struct FArcMassMakePlanMultipleGoalsTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassMakePlanMultipleGoalsTaskInstanceData;

	FArcMassMakePlanMultipleGoalsTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

};