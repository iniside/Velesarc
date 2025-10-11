// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcGI_IncrementInteger.h"

#include "StateTreeExecutionContext.h"

FArcGI_IncrementIntegerTask::FArcGI_IncrementIntegerTask()
{
	bShouldCallTick = false;
}

EStateTreeRunStatus FArcGI_IncrementIntegerTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	int32* Counter = InstanceData.Counter.GetMutablePtr(Context);
	if (Counter)
	{
		(*Counter)++;
	}
	
	return EStateTreeRunStatus::Running;
}
