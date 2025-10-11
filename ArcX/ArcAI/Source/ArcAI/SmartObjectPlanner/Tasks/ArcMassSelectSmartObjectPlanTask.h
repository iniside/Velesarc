#pragma once
#include "StateTreePropertyRef.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanStep.h"
#include "Tasks/ArcMassStateTreeRunEnvQueryTask.h"

#include "ArcMassSelectSmartObjectPlanTask.generated.h"

USTRUCT()
struct FArcMassSelectSmartObjectPlanTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	FArcSmartObjectPlanResponse PlanInput;
	
	// Result of the query. If an array is binded, it will output all the created values otherwise it will output the best one.
	UPROPERTY(VisibleAnywhere, Category = Parameter, meta = (CanRefToArray))
	TStateTreePropertyRef<TArray<FArcSmartObjectPlanStep>> OutPlanSteps;
};

/**
* Task that runs an async environment query and outputs the result to an outside parameter. Supports Actor and vector types EQS.
* The task is usually run in a sibling state to the result user will be with the data being stored in the parent state's parameters.
* - Parent (Has an EQS result parameter)
*	- Run Env Query (If success go to Use Query Result)
*	- Use Query Result
*/
USTRUCT(meta = (DisplayName = "Arc Mass Select Smart Object Plan", Category = "Common"))
struct FArcMassSelectSmartObjectPlanTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassSelectSmartObjectPlanTaskInstanceData;

	FArcMassSelectSmartObjectPlanTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

};