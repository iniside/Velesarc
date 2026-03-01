// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAssignToAreaTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"

EStateTreeRunStatus FArcMassAssignToAreaTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bAssignmentSucceeded = false;

	if (!InstanceData.AreaHandle.IsValid() || InstanceData.SlotIndex == INDEX_NONE)
	{
		return EStateTreeRunStatus::Failed;
	}

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
	InstanceData.bAssignmentSucceeded = Subsystem->AssignToSlot(InstanceData.AreaHandle, InstanceData.SlotIndex, Entity);

	return InstanceData.bAssignmentSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}
