// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_InputReleased.h"

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

EStateTreeRunStatus FArcAbilityTask_InputReleased::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& /*Transition*/) const
{
	FInstanceDataType& Data = Context.GetInstanceData<FInstanceDataType>(*this);

	// Reset internal state before subscribing — clears residue from any prior activation
	// of the same task instance (grumpy-review #12).
	Data.bSawPress = false;
	Data.PressTime = 0.0;
	Data.Duration  = 0.f;

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

				const double Now = LambdaWorld->GetTimeSeconds();

				if (!InstData->bSawPress)
				{
					if (Input->PressedThisFrame.Contains(CapturedTag))
					{
						InstData->bSawPress = true;
						InstData->PressTime = Now;
					}
					return;
				}

				if (Input->ReleasedThisFrame.Contains(CapturedTag))
				{
					InstData->Duration = static_cast<float>(Now - InstData->PressTime);
					StrongContext.BroadcastDelegate(InstData->OnInputReleased);

					UMassSignalSubsystem* WakeSub = UWorld::GetSubsystem<UMassSignalSubsystem>(LambdaWorld);
					if (WakeSub)
					{
						WakeSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
					}
				}
			});

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_InputReleased::ExitState(
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
