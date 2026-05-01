// Copyright Lukasz Baran. All Rights Reserved.

#include "MassAbilities/ArcAbilityTask_WaitEnhancedInput.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Input/ArcCoreInputComponent.h"
#include "MassActorSubsystem.h"
#include "MassEntityManager.h"
#include "StateTreeAsyncExecutionContext.h"
#include "StateTreeExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Signals/ArcAbilitySignals.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

EStateTreeRunStatus FArcAbilityTask_WaitEnhancedInput::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	if (!InstanceData.InputAction)
	{
		return EStateTreeRunStatus::Failed;
	}

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (!ActorFragment || !ActorFragment->IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	APawn* Pawn = Cast<APawn>(ActorFragment->GetMutable());
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();
	if (!ArcIC)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (InstanceData.InputMappingContext)
	{
		APlayerController* PC = Pawn->GetController<APlayerController>();
		if (PC)
		{
			ULocalPlayer* LP = PC->GetLocalPlayer();
			if (LP)
			{
				UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
				if (Subsystem)
				{
					FModifyContextOptions Options;
					Options.bForceImmediately = true;
					Options.bIgnoreAllPressedKeysUntilRelease = false;
					Subsystem->AddMappingContext(InstanceData.InputMappingContext, 999, Options);
				}
			}
		}
	}

	InstanceData.PressedHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
		, ETriggerEvent::Started
		, [WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity](const FInputActionValue& Val)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			StrongContext.BroadcastDelegate(InstanceData->OnInputPressed);

			UWorld* World = StrongContext.GetOwner()->GetWorld();
			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
			if (SignalSub)
			{
				SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
			}
		}).GetHandle();

	InstanceData.TriggeredHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
		, ETriggerEvent::Triggered
		, [WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity](const FInputActionValue& Val)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			StrongContext.BroadcastDelegate(InstanceData->OnInputTriggered);

			UWorld* World = StrongContext.GetOwner()->GetWorld();
			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
			if (SignalSub)
			{
				SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
			}
		}).GetHandle();

	InstanceData.ReleasedHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
		, ETriggerEvent::Completed
		, [WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity](const FInputActionValue& Val)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			StrongContext.BroadcastDelegate(InstanceData->OnInputReleased);

			UWorld* World = StrongContext.GetOwner()->GetWorld();
			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
			if (SignalSub)
			{
				SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
			}
		}).GetHandle();

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_WaitEnhancedInput::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (!ActorFragment || !ActorFragment->IsValid())
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(ActorFragment->GetMutable());
	if (!Pawn)
	{
		return;
	}

	UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();
	if (ArcIC)
	{
		ArcIC->RemoveBindingByHandle(InstanceData.PressedHandle);
		ArcIC->RemoveBindingByHandle(InstanceData.TriggeredHandle);
		ArcIC->RemoveBindingByHandle(InstanceData.ReleasedHandle);
	}

	if (InstanceData.InputMappingContext)
	{
		APlayerController* PC = Pawn->GetController<APlayerController>();
		if (PC)
		{
			ULocalPlayer* LP = PC->GetLocalPlayer();
			if (LP)
			{
				UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
				if (Subsystem)
				{
					FModifyContextOptions Options;
					Options.bForceImmediately = true;
					Options.bIgnoreAllPressedKeysUntilRelease = false;
					Subsystem->RemoveMappingContext(InstanceData.InputMappingContext, Options);
				}
			}
		}
	}

	InstanceData.PressedHandle = INDEX_NONE;
	InstanceData.TriggeredHandle = INDEX_NONE;
	InstanceData.ReleasedHandle = INDEX_NONE;
}
