#include "ArcMassIsEntityValidCondition.h"

#include "MassStateTreeExecutionContext.h"

bool FArcMassIsEntityValidCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
	const FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	bool bResult = InstanceData.TargetInput.GetEntityHandle().IsValid();
	return bResult ^ bInvert;
}
