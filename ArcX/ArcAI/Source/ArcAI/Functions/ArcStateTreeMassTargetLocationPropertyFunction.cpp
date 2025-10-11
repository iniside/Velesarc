#include "ArcStateTreeMassTargetLocationPropertyFunction.h"

#include "StateTreeExecutionContext.h"

void FArcStateTreeMassTargetLocationPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Output.EndOfPathPosition = InstanceData.Input;
	InstanceData.Output.EndOfPathIntent = InstanceData.EndOfPathIntent;
}

void FStateTreeGetMassTargetLocationFromTransformPropertyFunctionI::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Output.EndOfPathPosition = InstanceData.Input.GetLocation();
	InstanceData.Output.EndOfPathIntent = InstanceData.EndOfPathIntent;
}

void FArcStateTreeGetLocationFromTransformPropertyFunction::Execute(FStateTreeExecutionContext& Context) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Output = InstanceData.Input.GetLocation();
}
