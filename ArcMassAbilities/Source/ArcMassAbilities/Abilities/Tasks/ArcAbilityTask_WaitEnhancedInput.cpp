// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_WaitEnhancedInput.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcMassAbilitiesLogs.h"
#include "EnhancedInputComponent.h"
#include "MassActorSubsystem.h"
#include "Mass/EntityManager.h"
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

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	if (!InstanceData.InputAction)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Input] WaitEnhancedInput FAILED: no InputAction entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
	if (!ActorFragment || !ActorFragment->IsValid())
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Input] WaitEnhancedInput FAILED: no actor fragment entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	APawn* Pawn = Cast<APawn>(ActorFragment->GetMutable());
	if (!Pawn)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Input] WaitEnhancedInput FAILED: actor is not a Pawn entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	UEnhancedInputComponent* ArcIC = Pawn->FindComponentByClass<UEnhancedInputComponent>();
	if (!ArcIC)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Input] WaitEnhancedInput FAILED: no EnhancedInputComponent entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
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

	ARCMA_VLOG(World, Log,
		TEXT("[Input] WaitEnhancedInput: bound Pressed/Triggered/Released entity=[%d:%d] action=%s"),
		Entity.Index, Entity.SerialNumber, *InstanceData.InputAction->GetName());

	InstanceData.PressedHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
		, ETriggerEvent::Started
		, [WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity, CapturedWorld = World](const FInputActionValue& Val)
		{
			ARCMA_VLOG(CapturedWorld, Log,
				TEXT("[Input] WaitEnhancedInput: Pressed entity=[%d:%d]"),
				CapturedEntity.Index, CapturedEntity.SerialNumber);

			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			StrongContext.BroadcastDelegate(InstanceData->OnInputPressed);

			UWorld* LambdaWorld = StrongContext.GetOwner()->GetWorld();
			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(LambdaWorld);
			if (SignalSub)
			{
				SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
			}
		}).GetHandle();

	InstanceData.TriggeredHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
		, ETriggerEvent::Triggered
		, [WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity, CapturedWorld = World](const FInputActionValue& Val)
		{
			ARCMA_VLOG(CapturedWorld, Log,
				TEXT("[Input] WaitEnhancedInput: Triggered entity=[%d:%d]"),
				CapturedEntity.Index, CapturedEntity.SerialNumber);

			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			StrongContext.BroadcastDelegate(InstanceData->OnInputTriggered);

			UWorld* LambdaWorld = StrongContext.GetOwner()->GetWorld();
			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(LambdaWorld);
			if (SignalSub)
			{
				SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
			}
		}).GetHandle();

	InstanceData.ReleasedHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
		, ETriggerEvent::Completed
		, [WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity, CapturedWorld = World](const FInputActionValue& Val)
		{
			ARCMA_VLOG(CapturedWorld, Log,
				TEXT("[Input] WaitEnhancedInput: Released entity=[%d:%d]"),
				CapturedEntity.Index, CapturedEntity.SerialNumber);

			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstanceData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			StrongContext.BroadcastDelegate(InstanceData->OnInputReleased);

			UWorld* LambdaWorld = StrongContext.GetOwner()->GetWorld();
			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(LambdaWorld);
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
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	ARCMA_VLOG(World, Log,
		TEXT("[Input] WaitEnhancedInput: unbound entity=[%d:%d]"),
		Entity.Index, Entity.SerialNumber);

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

	UEnhancedInputComponent* ArcIC = Pawn->FindComponentByClass<UEnhancedInputComponent>();
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
