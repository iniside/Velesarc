// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassClaimActionTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcSettlementSubsystem.h"

EStateTreeRunStatus FArcMassClaimActionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bClaimSucceeded = false;

	if (!InstanceData.ActionHandle.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcSettlementSubsystem* Subsystem = World->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	InstanceData.bClaimSucceeded = Subsystem->ClaimAction(InstanceData.ActionHandle, Entity);

	return InstanceData.bClaimSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

void FArcMassClaimActionTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.bClaimSucceeded || !InstanceData.ActionHandle.IsValid())
	{
		return;
	}

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return;
	}

	UArcSettlementSubsystem* Subsystem = World->GetSubsystem<UArcSettlementSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (bCompleteOnExit)
	{
		Subsystem->CompleteAction(InstanceData.ActionHandle);
	}
	else if (bReleaseOnExit)
	{
		Subsystem->CancelAction(InstanceData.ActionHandle);
	}
}
