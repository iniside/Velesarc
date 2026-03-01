// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassClaimAdvertisementTask.h"
#include "MassStateTreeExecutionContext.h"
#include "ArcKnowledgeSubsystem.h"

EStateTreeRunStatus FArcMassClaimAdvertisementTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bClaimSucceeded = false;

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

	const FMassEntityHandle Entity = MassCtx.GetEntity();
	InstanceData.bClaimSucceeded = Subsystem->ClaimAdvertisement(InstanceData.AdvertisementHandle, Entity);

	return InstanceData.bClaimSucceeded ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Failed;
}

void FArcMassClaimAdvertisementTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.bClaimSucceeded || !InstanceData.AdvertisementHandle.IsValid())
	{
		return;
	}

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return;
	}

	UArcKnowledgeSubsystem* Subsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	if (bCompleteOnExit)
	{
		Subsystem->CompleteAdvertisement(InstanceData.AdvertisementHandle);
	}
	else if (bReleaseOnExit)
	{
		Subsystem->CancelAdvertisement(InstanceData.AdvertisementHandle);
	}
}
