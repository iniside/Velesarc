// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassUseSmartObjectTask.h"

#include "GameplayBehavior.h"
#include "GameplayBehaviorConfig.h"
#include "GameplayBehaviorSmartObjectBehaviorDefinition.h"
#include "GameplayBehaviorSubsystem.h"
#include "MassActorSubsystem.h"
#include "MassAIBehaviorTypes.h"
#include "MassCommonFragments.h"
#include "MassEntitySubsystem.h"
#include "MassNavMeshNavigationUtils.h"
#include "MassSignalSubsystem.h"
#include "MassSmartObjectBehaviorDefinition.h"
#include "MassSmartObjectHandler.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "StateTreeLinker.h"

FArcMassUseSmartObjectTask::FArcMassUseSmartObjectTask()
{
	bShouldCallTick = true;
}

bool FArcMassUseSmartObjectTask::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(SmartObjectSubsystemHandle);
	Linker.LinkExternalData(MassSignalSubsystemHandle);

	Linker.LinkExternalData(SmartObjectUserHandle);
	Linker.LinkExternalData(MoveTargetHandle);
	return true;
}

void FArcMassUseSmartObjectTask::GetDependencies(UE::MassBehavior::FStateTreeDependencyBuilder& Builder) const
{
	Builder.AddReadWrite(SmartObjectSubsystemHandle);
	Builder.AddReadWrite(MassSignalSubsystemHandle);

	Builder.AddReadWrite(SmartObjectUserHandle);
	Builder.AddReadWrite(MoveTargetHandle);
}

EStateTreeRunStatus FArcMassUseSmartObjectTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const UWorld* World = Context.GetWorld();

	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();

	UMassEntitySubsystem* MassEntitySubsystem = Context.GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	// Actor is optional — not all Mass entities have one
	FMassActorFragment* ActorFragment = MassCtx.GetEntityManager().GetFragmentDataPtr<FMassActorFragment>(MassCtx.GetEntity());
	AActor* Interactor = ActorFragment ? const_cast<AActor*>(ActorFragment->Get()) : nullptr;

	bool bHaveRunBehavior = false;

	// GameplayInteraction — check definition presence, actor is optional
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

	// GameplayBehavior — requires an actor
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

			const bool bBehaviorActive = UGameplayBehaviorSubsystem::TriggerBehavior(*InstanceData.GameplayBehavior
				, *Interactor, GameplayBehaviorConfig, nullptr);

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

	// MassBehavior
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

EStateTreeRunStatus FArcMassUseSmartObjectTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (InstanceData.GameplayInteractionContext.IsValid())
	{
		FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);

		const bool bStillRunning = InstanceData.GameplayInteractionContext.Tick(DeltaTime, &MassCtx.GetMassEntityExecutionContext());
		if (!bStillRunning)
		{
			return EStateTreeRunStatus::Succeeded;
		}
	}

	return EStateTreeRunStatus::Running;
}

void FArcMassUseSmartObjectTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FMassStateTreeExecutionContext& MassCtx = static_cast<FMassStateTreeExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.GameplayBehavior = nullptr;
	if (InstanceData.GameplayInteractionContext.IsValid())
	{
		InstanceData.GameplayInteractionContext.Deactivate(&MassCtx.GetMassEntityExecutionContext());
	}

	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	if (SOUser.InteractionHandle.IsValid())
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with a valid InteractionHandle: stop using the smart object."));

		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassCtx.GetMassEntityExecutionContext(), SmartObjectSubsystem, SignalSubsystem);
		MassSmartObjectHandler.StopUsingSmartObject(MassCtx.GetEntity(), SOUser, EMassSmartObjectInteractionStatus::Aborted);
	}
	else
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with an invalid ClaimHandle: nothing to do."));
	}

	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	SOSubsystem->Release(InstanceData.SmartObjectClaimHandle);
}

#if WITH_EDITOR
FText FArcMassUseSmartObjectTask::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return NSLOCTEXT("ArcAI", "UseSmartObjectDesc", "Use Smart Object");
}
#endif
