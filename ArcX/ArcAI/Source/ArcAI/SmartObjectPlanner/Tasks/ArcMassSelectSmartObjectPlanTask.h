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

	// Plan response to select from (typically bound from a Make Plan task output)
	UPROPERTY(EditAnywhere, Category = Input)
	FArcSmartObjectPlanResponse PlanInput;

	// Steps from the selected plan, output for sequential execution
	UPROPERTY(VisibleAnywhere, Category = Output, meta = (CanRefToArray))
	TArray<FArcSmartObjectPlanStep> OutPlanSteps;
};

/**
* Randomly selects one plan from a PlanResponse and outputs its steps.
* Separated from Make Plan tasks so that re-entering the state does not
* re-roll a different plan while the current one is still being executed.
* Should only be re-entered after the current plan finishes (success or fail).
*/
USTRUCT(meta = (DisplayName = "Arc Mass Select Smart Object Plan", Category = "Smart Object Planner"))
struct FArcMassSelectSmartObjectPlanTask : public FMassStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMassSelectSmartObjectPlanTaskInstanceData;

	FArcMassSelectSmartObjectPlanTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};