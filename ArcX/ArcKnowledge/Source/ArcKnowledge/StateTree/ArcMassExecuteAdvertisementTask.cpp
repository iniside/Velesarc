// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassExecuteAdvertisementTask.h"
#include "ArcAdvertisementInstruction_StateTree.h"
#include "ArcKnowledgeSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassEntityFragments.h"
#include "MassSignalSubsystem.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeLinker.h"

FArcMassExecuteAdvertisementTask::FArcMassExecuteAdvertisementTask()
{
	bShouldCallTick = false;
}

bool FArcMassExecuteAdvertisementTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TransformHandle);
	Linker.LinkExternalData(SignalSubsystemHandle);
	return true;
}

void FArcMassExecuteAdvertisementTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite(SignalSubsystemHandle);
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

	// Activate the StateTree
	if (!InstanceData.ExecutionContext.Activate(*Instruction))
	{
		UE_LOG(LogArcAdvertisement, Warning, TEXT("ExecuteAdvertisementTask: Failed to activate advertisement StateTree."));
		return EStateTreeRunStatus::Failed;
	}

	// Register ticker for per-frame updates (same pattern as ArcMassUseSmartObjectTask)
	UMassSignalSubsystem* SignalSubsystem = &Context.GetExternalData(SignalSubsystemHandle);
	const bool bCompleteOnSuccessLocal = bCompleteOnSuccess;
	const bool bCancelOnInterruptLocal = bCancelOnInterrupt;
	const FArcKnowledgeHandle AdvHandle = InstanceData.AdvertisementHandle;

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(KnowledgeSubsystem,
		[SignalSubsystem, KnowledgeSubsystem, WeakContext = Context.MakeWeakExecutionContext(),
		 EntityHandle, AdvHandle, bCompleteOnSuccessLocal, bCancelOnInterruptLocal](float DeltaTime)
		{
			const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!DataPtr)
			{
				return false;
			}

			const bool bKeepTicking = DataPtr->ExecutionContext.Tick(DeltaTime);
			if (!bKeepTicking)
			{
				const EStateTreeRunStatus FinalStatus = DataPtr->ExecutionContext.GetLastRunStatus();

				if (bCompleteOnSuccessLocal && FinalStatus == EStateTreeRunStatus::Succeeded)
				{
					KnowledgeSubsystem->CompleteAdvertisement(AdvHandle);
				}
				else if (bCancelOnInterruptLocal)
				{
					KnowledgeSubsystem->CancelAdvertisement(AdvHandle);
				}

				StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);
				SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {EntityHandle});
				return false;
			}

			return true;
		}));

	return EStateTreeRunStatus::Running;
}

void FArcMassExecuteAdvertisementTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.ExecutionContext.IsValid())
	{
		InstanceData.ExecutionContext.Deactivate();
	}
}
