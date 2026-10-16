// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_InputPressed.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Mass/EntityHandle.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"
#include "StateTreeAsyncExecutionContext.h"

EStateTreeRunStatus FArcAbilityTask_InputPressed::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& /*Transition*/) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	const FMassEntityHandle Entity = AbilityContext.GetEntity();
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
	if (!SignalSub)
	{
		return EStateTreeRunStatus::Failed;
	}

	FGameplayTag EffectiveTag = Data.InputTag;
	if (!EffectiveTag.IsValid())
	{
		FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
		const FArcActiveAbility* ActiveAbility =
			ArcAbilities::FindAbility(EntityManager, Entity, AbilityContext.GetAbilityHandle());
		if (ActiveAbility)
		{
			EffectiveTag = ActiveAbility->InputTag;
		}
	}

	Data.SignalHandle = SignalSub->GetSignalDelegateByName(UE::ArcMassAbilities::Signals::AbilityActiveInputReceived)
		.AddLambda(
			[WeakContext = Context.MakeWeakExecutionContext(),
			 CapturedEntity = Entity,
			 CapturedTag = EffectiveTag]
			(FName /*SignalName*/, TConstArrayView<FMassEntityHandle> Entities)
			{
				if (!Entities.Contains(CapturedEntity))
				{
					return;
				}

				FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
				FInstanceDataType* InstData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
				if (!InstData)
				{
					return;
				}

				UWorld* LambdaWorld = StrongContext.GetOwner()->GetWorld();
				UMassEntitySubsystem* MassSub = UWorld::GetSubsystem<UMassEntitySubsystem>(LambdaWorld);
				if (!MassSub)
				{
					return;
				}

				FMassEntityManager& EntMgr = MassSub->GetMutableEntityManager();
				const FMassEntityView View(EntMgr, CapturedEntity);
				const FArcAbilityInputFragment* Input = View.GetSparseFragmentDataPtr<FArcAbilityInputFragment>();
				if (!Input)
				{
					return;
				}

				if (!Input->PressedThisFrame.Contains(CapturedTag))
				{
					return;
				}

				StrongContext.BroadcastDelegate(InstData->OnInputPressed);

				UMassSignalSubsystem* WakeSub = UWorld::GetSubsystem<UMassSignalSubsystem>(LambdaWorld);
				if (WakeSub)
				{
					WakeSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
				}
			});

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_InputPressed::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& /*Transition*/) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
	if (SignalSub && Data.SignalHandle.IsValid())
	{
		SignalSub->GetSignalDelegateByName(UE::ArcMassAbilities::Signals::AbilityActiveInputReceived)
			.Remove(Data.SignalHandle);
	}
	Data.SignalHandle.Reset();
}
