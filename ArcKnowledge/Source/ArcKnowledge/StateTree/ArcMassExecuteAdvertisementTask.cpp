// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassExecuteAdvertisementTask.h"
#include "ArcAdvertisementInstruction_StateTree.h"
#include "ArcKnowledgeSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassExecuteAdvertisementTask::FArcMassExecuteAdvertisementTask()
{
	bShouldCallTick = true;
}

bool FArcMassExecuteAdvertisementTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	return true;
}

EStateTreeRunStatus FArcMassExecuteAdvertisementTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.AdvertisementHandle.IsValid())
	{
		UE_LOG(LogArcAdvertisement, Warning, TEXT("ExecuteAdvertisementTask: Invalid advertisement handle."));
		return EStateTreeRunStatus::Failed;
	}

	UWorld* World = MassCtx.GetWorld();
	if (!World)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcKnowledgeSubsystem* KnowledgeSubsystem = World->GetSubsystem<UArcKnowledgeSubsystem>();
	if (!KnowledgeSubsystem)
	{
		return EStateTreeRunStatus::Failed;
	}

	// Look up the advertisement entry
	const FArcKnowledgeEntry* Entry = KnowledgeSubsystem->GetKnowledgeEntry(InstanceData.AdvertisementHandle);
	if (!Entry)
	{
		UE_LOG(LogArcAdvertisement, Warning, TEXT("ExecuteAdvertisementTask: Advertisement entry not found for handle."));
		return EStateTreeRunStatus::Failed;
	}

	// Extract the StateTree instruction
	const FArcAdvertisementInstruction_StateTree* Instruction = Entry->Instruction.GetPtr<FArcAdvertisementInstruction_StateTree>();
	if (!Instruction || !Instruction->StateTreeReference.GetStateTree())
	{
		UE_LOG(LogArcAdvertisement, Warning, TEXT("ExecuteAdvertisementTask: Advertisement has no valid StateTree instruction."));
		return EStateTreeRunStatus::Failed;
	}

	// Get the actor for this entity (optional — not all Mass entities have actors)
	const FMassEntityHandle EntityHandle = MassCtx.GetEntity();
	AActor* ContextActor = nullptr;
	if (const FMassActorFragment* ActorFragment = MassCtx.GetEntityManager().GetFragmentDataPtr<FMassActorFragment>(EntityHandle))
	{
		ContextActor = const_cast<AActor*>(ActorFragment->Get());
	}

	// Set up the execution context — Owner is the subsystem, ContextActor is optional
	InstanceData.ExecutionContext.SetOwner(KnowledgeSubsystem);
	InstanceData.ExecutionContext.SetExecutingEntity(EntityHandle);
	InstanceData.ExecutionContext.SetSourceEntity(Entry->SourceEntity);
	InstanceData.ExecutionContext.SetContextActor(ContextActor);
	InstanceData.ExecutionContext.SetAdvertisementHandle(InstanceData.AdvertisementHandle);
	InstanceData.ExecutionContext.SetAdvertisementPayload(Entry->Payload);

	// Activate the StateTree — pass Mass execution context for fragment resolution
	if (!InstanceData.ExecutionContext.Activate(*Instruction, &MassCtx.GetMassEntityExecutionContext()))
	{
		UE_LOG(LogArcAdvertisement, Warning, TEXT("ExecuteAdvertisementTask: Failed to activate advertisement StateTree."));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FArcMassExecuteAdvertisementTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const bool bStillRunning = InstanceData.ExecutionContext.Tick(DeltaTime, &MassCtx.GetMassEntityExecutionContext());
	return bStillRunning ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
}

void FArcMassExecuteAdvertisementTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.AdvertisementHandle.IsValid())
	{
		return;
	}

	const EStateTreeRunStatus InnerStatus = InstanceData.ExecutionContext.GetLastRunStatus();
	const bool bInterrupted = (Transition.CurrentRunStatus == EStateTreeRunStatus::Running);

	UWorld* World = MassCtx.GetWorld();
	UArcKnowledgeSubsystem* KnowledgeSubsystem = World ? World->GetSubsystem<UArcKnowledgeSubsystem>() : nullptr;

	if (KnowledgeSubsystem)
	{
		if (!bInterrupted && bCompleteOnSuccess && InnerStatus == EStateTreeRunStatus::Succeeded)
		{
			KnowledgeSubsystem->CompleteAdvertisement(InstanceData.AdvertisementHandle);
		}
		else if (bInterrupted && bCancelOnInterrupt)
		{
			KnowledgeSubsystem->CancelAdvertisement(InstanceData.AdvertisementHandle);
		}
	}

	if (InstanceData.ExecutionContext.IsValid())
	{
		InstanceData.ExecutionContext.Deactivate(&MassCtx.GetMassEntityExecutionContext());
	}
}
