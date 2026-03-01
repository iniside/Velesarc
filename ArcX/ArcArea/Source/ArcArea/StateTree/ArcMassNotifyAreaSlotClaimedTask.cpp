// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassNotifyAreaSlotClaimedTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcAreaSubsystem.h"
#include "Mass/ArcAreaAssignmentFragments.h"

EStateTreeRunStatus FArcMassNotifyAreaSlotClaimedTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

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

	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	const FArcAreaAssignmentFragment* Assignment = EntityManager.GetFragmentDataPtr<FArcAreaAssignmentFragment>(Entity);
	if (!Assignment || !Assignment->IsAssigned())
	{
		return EStateTreeRunStatus::Failed;
	}

	Subsystem->NotifySlotClaimed(Assignment->AreaHandle, Assignment->SlotIndex);

	return EStateTreeRunStatus::Running;
}

void FArcMassNotifyAreaSlotClaimedTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return;
	}

	UArcAreaSubsystem* Subsystem = World->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
	const FMassEntityHandle Entity = MassCtx.GetEntity();

	const FArcAreaAssignmentFragment* Assignment = EntityManager.GetFragmentDataPtr<FArcAreaAssignmentFragment>(Entity);
	if (!Assignment || !Assignment->IsAssigned())
	{
		return;
	}

	Subsystem->NotifySlotReleased(Assignment->AreaHandle, Assignment->SlotIndex);
}
