// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassUnclaimAdvertisementTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcKnowledgeSubsystem.h"

EStateTreeRunStatus FArcMassUnclaimAdvertisementTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bSuccess = false;

	if (!InstanceData.AdvertisementHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.bSuccess = Subsystem->CancelAdvertisement(InstanceData.AdvertisementHandle);
	return InstanceData.bSuccess ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
