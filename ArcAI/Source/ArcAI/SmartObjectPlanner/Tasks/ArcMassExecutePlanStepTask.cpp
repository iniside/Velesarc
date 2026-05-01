// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassExecutePlanStepTask.h"

#include "ArcAILogs.h"
#include "ArcAdvertisementInstruction_StateTree.h"
#include "ArcKnowledgeSubsystem.h"
#include "GameplayBehavior.h"
#include "GameplayBehaviorConfig.h"
#include "GameplayBehaviorSmartObjectBehaviorDefinition.h"
#include "GameplayBehaviorSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "Mass/EntityFragments.h"
#include "MassEntitySubsystem.h"
#include "MassNavMeshNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectBehaviorDefinition.h"
#include "MassSmartObjectFragments.h"
#include "MassSmartObjectHandler.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"

FArcMassExecutePlanStepTask::FArcMassExecutePlanStepTask()
{
	bShouldCallTick = true;
}

bool FArcMassExecutePlanStepTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);
	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	Linker.LinkExternalData(TransformHandle);
	return true;
}

void FArcMassExecutePlanStepTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite(SmartObjectSubsystemHandle);
	Builder.AddReadWrite(MassSignalSubsystemHandle);
	Builder.AddReadWrite(SmartObjectUserHandle);
	Builder.AddReadWrite(MoveTargetHandle);
	Builder.AddReadOnly(TransformHandle);
}

// ─────────────────────────────────────────────────────────────
// Knowledge path helpers
// ─────────────────────────────────────────────────────────────

namespace ArcExecutePlanStep
{
	EStateTreeRunStatus EnterKnowledgePath(
		FMassStateTreeExecutionContext& MassCtx,
		FArcMassExecutePlanStepTaskInstanceData& InstanceData)
	{
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

		const FArcKnowledgeEntry* Entry = KnowledgeSubsystem->GetKnowledgeEntry(InstanceData.AdvertisementHandle);
		if (!Entry)
		{
			UE_LOG(LogGoalPlannerStep, Warning, TEXT("ExecutePlanStep: Knowledge entry not found for handle."));
			return EStateTreeRunStatus::Failed;
		}

		const FArcAdvertisementInstruction_StateTree* Instruction = Entry->Instruction.GetPtr<FArcAdvertisementInstruction_StateTree>();
		if (!Instruction || !Instruction->StateTreeReference.GetStateTree())
		{
			UE_LOG(LogGoalPlannerStep, Warning, TEXT("ExecutePlanStep: Knowledge entry has no valid StateTree instruction."));
			return EStateTreeRunStatus::Failed;
		}

		const FMassEntityHandle EntityHandle = MassCtx.GetEntity();

		AActor* ContextActor = nullptr;
		if (const FMassActorFragment* ActorFragment = MassCtx.GetEntityManager().GetFragmentDataPtr<FMassActorFragment>(EntityHandle))
		{
			ContextActor = const_cast<AActor*>(ActorFragment->Get());
		}

		FVector SourceEntityLocation = FVector::ZeroVector;
		if (Entry->SourceEntity.IsValid())
		{
			const FMassEntityManager& EntityManager = MassCtx.GetEntityManager();
			if (const FTransformFragment* SourceTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entry->SourceEntity))
			{
				SourceEntityLocation = SourceTransform->GetTransform().GetLocation();
			}
		}

		InstanceData.AdvertisementExecutionContext.SetOwner(KnowledgeSubsystem);
		InstanceData.AdvertisementExecutionContext.SetExecutingEntity(EntityHandle);
		InstanceData.AdvertisementExecutionContext.SetSourceEntity(Entry->SourceEntity);
		InstanceData.AdvertisementExecutionContext.SetContextActor(ContextActor);
		InstanceData.AdvertisementExecutionContext.SetAdvertisementHandle(InstanceData.AdvertisementHandle);
		InstanceData.AdvertisementExecutionContext.SetAdvertisementPayload(Entry->Payload);
		InstanceData.AdvertisementExecutionContext.SetKnowledgeLocation(Entry->Location);
		InstanceData.AdvertisementExecutionContext.SetSourceEntityLocation(SourceEntityLocation);

		if (!InstanceData.AdvertisementExecutionContext.Activate(*Instruction, &MassCtx.GetMassEntityExecutionContext()))
		{
			UE_LOG(LogGoalPlannerStep, Warning, TEXT("ExecutePlanStep: Failed to activate advertisement StateTree."));
			return EStateTreeRunStatus::Failed;
		}

		return EStateTreeRunStatus::Running;
	}
}

// ─────────────────────────────────────────────────────────────
// EnterState
// ─────────────────────────────────────────────────────────────

EStateTreeRunStatus FArcMassExecutePlanStepTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// --- Knowledge path ---
	if (InstanceData.AdvertisementHandle.IsValid())
	{
		return ArcExecutePlanStep::EnterKnowledgePath(MassCtx, InstanceData);
	}

	// --- SmartObject path (mirrors FArcMassUseSmartObjectTask) ---
	const UWorld* World = Context.GetWorld();
	USmartObjectSubsystem* SOSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
	UMassEntitySubsystem* MassEntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	FMassActorFragment* ActorFragment = MassCtx.GetEntityManager().GetFragmentDataPtr<FMassActorFragment>(MassCtx.GetEntity());
	AActor* Interactor = ActorFragment ? const_cast<AActor*>(ActorFragment->Get()) : nullptr;

	bool bHaveRunBehavior = false;

	const UArcAIGameplayInteractionSmartObjectBehaviorDefinition* GI = SOSubsystem->GetBehaviorDefinition<UArcAIGameplayInteractionSmartObjectBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
	if (GI)
	{
		InstanceData.GameplayInteractionContext.SetOwner(SOSubsystem);
		InstanceData.GameplayInteractionContext.SetContextEntity(MassCtx.GetEntity());
		InstanceData.GameplayInteractionContext.SetSmartObjectEntity(InstanceData.SmartObjectEntityHandle.EntityHandle);
		InstanceData.GameplayInteractionContext.SetContextActor(Interactor);
		InstanceData.GameplayInteractionContext.SetSmartObjectActor(Interactor);
		InstanceData.GameplayInteractionContext.SetClaimedHandle(InstanceData.SmartObjectClaimHandle);

		if (const TOptional<FTransform> SlotTransform = SOSubsystem->GetSlotTransform(InstanceData.SmartObjectClaimHandle))
		{
			InstanceData.GameplayInteractionContext.SetSlotTransform(SlotTransform.GetValue());
		}

		if (!InstanceData.GameplayInteractionContext.Activate(*GI, &MassCtx.GetMassEntityExecutionContext()))
		{
			return EStateTreeRunStatus::Failed;
		}

		bHaveRunBehavior = true;
	}

	if (Interactor)
	{
		const UGameplayBehaviorSmartObjectBehaviorDefinition* BD = SOSubsystem->GetBehaviorDefinition<UGameplayBehaviorSmartObjectBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
		if (BD)
		{
			const UGameplayBehaviorConfig* GameplayBehaviorConfig = BD->GameplayBehaviorConfig.Get();
			InstanceData.GameplayBehavior = GameplayBehaviorConfig != nullptr ? GameplayBehaviorConfig->GetBehavior(*Context.GetWorld()) : nullptr;

			if (!InstanceData.GameplayBehavior)
			{
				return EStateTreeRunStatus::Failed;
			}

			const bool bBehaviorActive = UGameplayBehaviorSubsystem::TriggerBehavior(*InstanceData.GameplayBehavior, *Interactor, GameplayBehaviorConfig, nullptr);

			if (bBehaviorActive)
			{
				InstanceData.GameplayBehavior->GetOnBehaviorFinishedDelegate().AddWeakLambda(MassEntitySubsystem, [SOSubsystem, WeakContext = Context.MakeWeakExecutionContext(), EntityHandle = MassCtx.GetEntity(), SignalSubsystem]
					(UGameplayBehavior& InBehavior, AActor& /*Avatar*/, const bool /*bInterrupted*/)
				{
					const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
					FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
					if (DataPtr)
					{
						SOSubsystem->Release(DataPtr->SmartObjectClaimHandle);
						DataPtr->GameplayBehavior = nullptr;
						StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

						SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {EntityHandle});
					}
				});

				bHaveRunBehavior = true;
			}
		}
	}

	bool bRunMassBehavior = bAlwaysRunMassBehavior || !bHaveRunBehavior;

	const USmartObjectMassBehaviorDefinition* MBD = SOSubsystem->GetBehaviorDefinition<USmartObjectMassBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
	if (bRunMassBehavior && MBD)
	{
		const FMassSmartObjectHandler MassSmartObjectHandler(MassCtx.GetMassEntityExecutionContext(), *SOSubsystem, *SignalSubsystem);

		FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

		if (SOUser.InteractionHandle.IsValid())
		{
			return bHaveRunBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}

		if (!MassSmartObjectHandler.StartUsingSmartObject(MassCtx.GetEntity(), SOUser, InstanceData.SmartObjectClaimHandle))
		{
			return bHaveRunBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}

		FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);
		MoveTarget.CreateNewAction(EMassMovementAction::Animate, *World);
		UE::MassNavigation::ActivateActionAnimate(Interactor, MassCtx.GetEntity(), MoveTarget);

		return EStateTreeRunStatus::Running;
	}

	return bHaveRunBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

// ─────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────

EStateTreeRunStatus FArcMassExecutePlanStepTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// --- Knowledge path ---
	if (InstanceData.AdvertisementHandle.IsValid())
	{
		const bool bStillRunning = InstanceData.AdvertisementExecutionContext.Tick(DeltaTime, &MassCtx.GetMassEntityExecutionContext());
		return bStillRunning ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Succeeded;
	}

	// --- SmartObject path ---
	if (InstanceData.GameplayInteractionContext.IsValid())
	{
		const bool bStillRunning = InstanceData.GameplayInteractionContext.Tick(DeltaTime, &MassCtx.GetMassEntityExecutionContext());
		if (!bStillRunning)
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	return EStateTreeRunStatus::Running;
}

// ─────────────────────────────────────────────────────────────
// ExitState
// ─────────────────────────────────────────────────────────────

void FArcMassExecutePlanStepTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// --- Knowledge path ---
	if (InstanceData.AdvertisementHandle.IsValid())
	{
		const EStateTreeRunStatus InnerStatus = InstanceData.AdvertisementExecutionContext.GetLastRunStatus();
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

		if (InstanceData.AdvertisementExecutionContext.IsValid())
		{
			InstanceData.AdvertisementExecutionContext.Deactivate(&MassCtx.GetMassEntityExecutionContext());
		}

		return;
	}

	// --- SmartObject path ---
	InstanceData.GameplayBehavior = nullptr;
	if (InstanceData.GameplayInteractionContext.IsValid())
	{
		InstanceData.GameplayInteractionContext.Deactivate(&MassCtx.GetMassEntityExecutionContext());
	}

	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	if (SOUser.InteractionHandle.IsValid())
	{
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassCtx.GetMassEntityExecutionContext(), SmartObjectSubsystem, SignalSubsystem);
		MassSmartObjectHandler.StopUsingSmartObject(MassCtx.GetEntity(), SOUser, EMassSmartObjectInteractionStatus::Aborted);
	}

	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	SOSubsystem->Release(InstanceData.SmartObjectClaimHandle);
}

#if WITH_EDITOR
FText FArcMassExecutePlanStepTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "ExecutePlanStepDesc", "Execute Plan Step (SO or Knowledge)");
}
#endif
