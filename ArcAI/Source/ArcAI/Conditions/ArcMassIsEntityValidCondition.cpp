#include "ArcMassIsEntityValidCondition.h"

#include "MassStateTreeExecutionContext.h"

bool FArcMassIsEntityValidCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	bool bResult = InstanceData.TargetInput.GetEntityHandle().IsValid();
	return bResult ^ bInvert;
}

#if WITH_EDITOR
FText FArcMassIsEntityValidCondition::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return bInvert
		? NSLOCTEXT("ArcAI", "IsEntityInvalidDesc", "Entity Is NOT Valid")
		: NSLOCTEXT("ArcAI", "IsEntityValidDesc", "Entity Is Valid");
}
#endif
