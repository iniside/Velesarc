// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassUnassignFromAreaTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"

EStateTreeRunStatus FArcMassUnassignFromAreaTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bWasAssigned = false;

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcAreaSubsystem* Subsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	InstanceData.bWasAssigned = Subsystem->UnassignEntity(Entity);

	return EStateTreeRunStatus::Succeeded;
}
