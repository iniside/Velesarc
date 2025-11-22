#include "ArcSmartObjectHaveAnyPlanCondition.h"

#include "StateTreeExecutionContext.h"

bool FArcSmartObjectHaveAnyPlanCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool HasPlans = InstanceData.PlanInput.Plans.Num() > 0;
	return HasPlans ^ bInvert;
}
