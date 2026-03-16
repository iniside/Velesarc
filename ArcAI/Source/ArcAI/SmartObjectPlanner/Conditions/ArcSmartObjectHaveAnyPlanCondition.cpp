#include "ArcSmartObjectHaveAnyPlanCondition.h"

#include "StateTreeExecutionContext.h"

bool FArcSmartObjectHaveAnyPlanCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	bool HasPlans = InstanceData.PlanInput.Plans.Num() > 0;
	return HasPlans ^ bInvert;
}

#if WITH_EDITOR
FText FArcSmartObjectHaveAnyPlanCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return bInvert
		? NSLOCTEXT("ArcAI", "HaveNoPlanDesc", "Has NO Plan")
		: NSLOCTEXT("ArcAI", "HaveAnyPlanDesc", "Has Any Plan");
}
#endif
