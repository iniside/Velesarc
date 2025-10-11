#pragma once
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"
#include "StateTreePropertyRef.h"
#include "ArcMass/ArcMassEntityHandleWrapper.h"
#include "Tasks/ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcMassGetNextPlanStepTask.generated.h"

USTRUCT()
struct FArcMassGetNextPlanStepTaskInstanceData
{
	GENERATED_BODY()

	// Result of the query. If an array is binded, it will output all the created values otherwise it will output the best one.
	UPROPERTY(VisibleAnywhere, Category = Parameter, meta = (CanRefToArray))
	TStateTreePropertyRef<TArray<FArcSmartObjectPlanStep>> PlanItemsQueue;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TStateTreePropertyRef<FArcMassEntityHandleWrapper> SmartObjectEntityHandle;
	
	UPROPERTY(VisibleAnywhere, Category = Parameter)
	TStateTreePropertyRef<int32> CurrentStepIdx;

	UPROPERTY(VisibleAnywhere, Category = Parameter)
	TStateTreePropertyRef<FMassSmartObjectCandidateSlots> CandidateSlots;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnPlanFinished;
};

/**
* Task that runs an async environment query and outputs the result to an outside parameter. Supports Actor and vector types EQS.
* The task is usually run in a sibling state to the result user will be with the data being stored in the parent state's parameters.
* - Parent (Has an EQS result parameter)
*	- Run Env Query (If success go to Use Query Result)
*	- Use Query Result
*/
USTRUCT(meta = (DisplayName = "Arc Mass Get Next Plan Step Task", Category = "Common"))
struct FArcMassGetNextPlanStepTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassGetNextPlanStepTaskInstanceData;

	FArcMassGetNextPlanStepTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

};