// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassGetClaimedAdvertisementsTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcKnowledgeSubsystem.h"

EStateTreeRunStatus FArcMassGetClaimedAdvertisementsTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.ClaimedHandles.Reset();
	InstanceData.ClaimedCount = 0;

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

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	Subsystem->GetAdvertisementsClaimedBy(Entity, InstanceData.ClaimedHandles);
	InstanceData.ClaimedCount = InstanceData.ClaimedHandles.Num();

	return InstanceData.ClaimedCount > 0 ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
