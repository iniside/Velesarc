// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassStoreKnowledgeHandleTask.h"

EStateTreeRunStatus FArcMassStoreKnowledgeHandleTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.KnowledgeHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.StoredHandle = InstanceData.KnowledgeHandle;
	return EStateTreeRunStatus::Succeeded;
}
