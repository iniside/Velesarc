// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSetBoolTask.h"

FArcMassSetBoolTask::FArcMassSetBoolTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcMassSetBoolTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	
	if (!bSetOnExit)
	{
		bool* bValue = InstanceData.BoolToSet.GetMutablePtr<bool>(Context);
		if (bValue)
		{
			*bValue = InstanceData.bNewValue;
		}
	}
	
	
	return EStateTreeRunStatus::Running;
}

void FArcMassSetBoolTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	if (bSetOnExit)
	{
		bool* bValue = InstanceData.BoolToSet.GetMutablePtr<bool>(Context);
		if (bValue)
		{
			*bValue = InstanceData.bNewValue;
		}
	}
}
