#include "ArcStateTreeMassTargetLocationPropertyFunction.h"

#include "StateTreeExecutionContext.h"

void FArcStateTreeMassTargetLocationPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Output.EndOfPathPosition = InstanceData.Input;
	InstanceData.Output.EndOfPathIntent = InstanceData.EndOfPathIntent;
}
