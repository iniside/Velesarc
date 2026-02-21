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
	bShouldCallTick = false;
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

	FMassActorFragment* ActorFragment = MassCtx.GetEntityManager().GetFragmentDataPtr<FMassActorFragment>(MassCtx.GetEntity());
	AActor* Interactor = const_cast<AActor*>(ActorFragment->Get());

	bool bHaveRunActorBehavior = false;
	if (Interactor)
	{
		const UArcAIGameplayInteractionSmartObjectBehaviorDefinition* GI = SOSubsystem->GetBehaviorDefinition<UArcAIGameplayInteractionSmartObjectBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
		if (GI)
		{
			InstanceData.GameplayInteractionContext.SetContextEntity(MassCtx.GetEntity());
			InstanceData.GameplayInteractionContext.SetSmartObjectEntity(InstanceData.SmartObjectEntityHandle.EntityHandle);

			InstanceData.GameplayInteractionContext.SetContextActor(Interactor);
			InstanceData.GameplayInteractionContext.SetSmartObjectActor(Interactor);
			InstanceData.GameplayInteractionContext.SetClaimedHandle(InstanceData.SmartObjectClaimHandle);

			InstanceData.GameplayInteractionContext.Activate(*GI);

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(Interactor, [SignalSubsystem, SOSubsystem, Interactor, WeakContext = Context.MakeWeakExecutionContext(), EntityHandle = MassCtx.GetEntity()](float DeltaTime)
			{
				const FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
				FInstanceDataType* DataPtr = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
					if (!DataPtr)
					{
						return false;
					}

					const bool bKeepTicking = DataPtr->GameplayInteractionContext.Tick(DeltaTime);
					if (bKeepTicking == false)
					{
						SOSubsystem->Release(DataPtr->SmartObjectClaimHandle);
						StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

						SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {EntityHandle});
						return false;;
					}

					return true;
			}));

			bHaveRunActorBehavior = true;
		}

		const UGameplayBehaviorSmartObjectBehaviorDefinition* BD = SOSubsystem->GetBehaviorDefinition<UGameplayBehaviorSmartObjectBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
		if (BD)
		{
			const UGameplayBehaviorConfig* GameplayBehaviorConfig = BD != nullptr ? BD->GameplayBehaviorConfig.Get() : nullptr;
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
					//if (DataPtr && DataPtr->GameplayBehavior == &InBehavior)
					{
						SOSubsystem->Release(DataPtr->SmartObjectClaimHandle);
						DataPtr->GameplayBehavior = nullptr;
						StrongContext.FinishTask(EStateTreeFinishTaskType::Succeeded);

						SignalSubsystem->SignalEntities(UE::Mass::Signals::NewStateTreeTaskRequired, {EntityHandle});
					}
				});

				bHaveRunActorBehavior = true;
			}
		}
	}


	bool bRunMassBehavior = false;
	if (bAlwaysRunMassBehavior || bHaveRunActorBehavior == false)
	{
		bRunMassBehavior = true;
	}

	const USmartObjectMassBehaviorDefinition* MBD = SOSubsystem->GetBehaviorDefinition<USmartObjectMassBehaviorDefinition>(InstanceData.SmartObjectClaimHandle);
	if (bRunMassBehavior && MBD)
	{
		const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassStateTreeContext.GetMassEntityExecutionContext(), *SOSubsystem, *SignalSubsystem);

		FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

		if (SOUser.InteractionHandle.IsValid())
		{
			//MASSBEHAVIOR_LOG(Error, TEXT("Agent is already using smart object slot %s."), *LexToString(SOUser.InteractionHandle));
			return bHaveRunActorBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}

		if (!MassSmartObjectHandler.StartUsingSmartObject(MassStateTreeContext.GetEntity(), SOUser, InstanceData.SmartObjectClaimHandle))
		{
			return bHaveRunActorBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
		}

		FMassMoveTargetFragment& MoveTarget = Context.GetExternalData(MoveTargetHandle);

		MoveTarget.CreateNewAction(EMassMovementAction::Animate, *World);

		bool bSuccess = UE::MassNavigation::ActivateActionAnimate(Interactor, MassStateTreeContext.GetEntity(), MoveTarget);

		return bSuccess ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
	}

	return bHaveRunActorBehavior ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}

void FArcMassUseSmartObjectTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transitio) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.GameplayBehavior = nullptr;
	if (InstanceData.GameplayInteractionContext.IsValid())
	{
		InstanceData.GameplayInteractionContext.Deactivate();
	}

	FMassSmartObjectUserFragment& SOUser = Context.GetExternalData(SmartObjectUserHandle);

	if (SOUser.InteractionHandle.IsValid())
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with a valid InteractionHandle: stop using the smart object."));

		const FMassStateTreeExecutionContext& MassStateTreeContext = static_cast<FMassStateTreeExecutionContext&>(Context);
		USmartObjectSubsystem& SmartObjectSubsystem = Context.GetExternalData(SmartObjectSubsystemHandle);
		UMassSignalSubsystem& SignalSubsystem = Context.GetExternalData(MassSignalSubsystemHandle);
		const FMassSmartObjectHandler MassSmartObjectHandler(MassStateTreeContext.GetMassEntityExecutionContext(), SmartObjectSubsystem, SignalSubsystem);
		MassSmartObjectHandler.StopUsingSmartObject(MassStateTreeContext.GetEntity(), SOUser, EMassSmartObjectInteractionStatus::Aborted);
	}
	else
	{
		MASSBEHAVIOR_LOG(VeryVerbose, TEXT("Exiting state with an invalid ClaimHandle: nothing to do."));
	}

	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	SOSubsystem->Release(InstanceData.SmartObjectClaimHandle);
}
