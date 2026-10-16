// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_WaitEnhancedInputGated.h"

#include "Abilities/Schema/ArcAbilityExecutionContext.h"
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
#include "ArcMassAbilitiesLogs.h"

namespace ArcAbilityTask_WaitEnhancedInputGatedInternal
{
	static APawn* ResolvePawn(FMassEntityManager& EntityManager, FMassEntityHandle Entity)
	{
		FMassActorFragment* ActorFragment = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity);
		if (!ActorFragment || !ActorFragment->IsValid())
		{
			return nullptr;
		}
		return Cast<APawn>(ActorFragment->GetMutable());
	}
}

EStateTreeRunStatus FArcAbilityTask_WaitEnhancedInputGated::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	InstanceData.bWasPressed = false;
	InstanceData.HeldDuration = 0.f;

	if (!InstanceData.InputAction)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Input] WaitEnhancedInputGated FAILED: no InputAction entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	APawn* Pawn = ArcAbilityTask_WaitEnhancedInputGatedInternal::ResolvePawn(EntityManager, Entity);
	if (!Pawn)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Input] WaitEnhancedInputGated FAILED: no pawn entity=[%d:%d]"),
			Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	UEnhancedInputComponent* ArcIC = Pawn->FindComponentByClass<UEnhancedInputComponent>();
	if (!ArcIC)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Input] WaitEnhancedInputGated FAILED: no UEnhancedInputComponent entity=[%d:%d]"),
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
					Subsystem->AddMappingContext(InstanceData.InputMappingContext, InstanceData.MappingContextPriority, Options);
				}
			}
		}
	}

	ARCMA_VLOG(World, Log,
		TEXT("[Input] WaitEnhancedInputGated enter entity=[%d:%d]"),
		Entity.Index, Entity.SerialNumber);

	InstanceData.PressedHandle = ArcIC->BindActionValueLambda(InstanceData.InputAction
		, ETriggerEvent::Started
		, [WeakContext = Context.MakeWeakExecutionContext(), CapturedEntity = Entity](const FInputActionValue& Val)
		{
			FStateTreeStrongExecutionContext StrongContext = WeakContext.MakeStrongExecutionContext();
			FInstanceDataType* InstData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstData)
			{
				return;
			}

			UWorld* World = StrongContext.GetOwner()->GetWorld();

			InstData->bWasPressed = true;
			InstData->PressTimeSeconds = World->GetTimeSeconds();

			ARCMA_VLOG(World, Log,
				TEXT("[Input] WaitEnhancedInputGated: press ACCEPTED entity=[%d:%d]"),
				CapturedEntity.Index, CapturedEntity.SerialNumber);

			StrongContext.BroadcastDelegate(InstData->OnInputPressed);

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
			FInstanceDataType* InstData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstData)
			{
				return;
			}

			if (!InstData->bWasPressed)
			{
				return;
			}

			UWorld* World = StrongContext.GetOwner()->GetWorld();

			StrongContext.BroadcastDelegate(InstData->OnInputTriggered);

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
			FInstanceDataType* InstData = StrongContext.GetInstanceDataPtr<FInstanceDataType>();
			if (!InstData)
			{
				return;
			}

			if (!InstData->bWasPressed)
			{
				return;
			}

			UWorld* World = StrongContext.GetOwner()->GetWorld();

			InstData->HeldDuration = static_cast<float>(World->GetTimeSeconds() - InstData->PressTimeSeconds);
			InstData->bWasPressed = false;

			ARCMA_VLOG(World, Log,
				TEXT("[Input] WaitEnhancedInputGated: released held_duration=%.2f entity=[%d:%d]"),
				InstData->HeldDuration, CapturedEntity.Index, CapturedEntity.SerialNumber);

			StrongContext.BroadcastDelegate(InstData->OnInputReleased);

			UMassSignalSubsystem* SignalSub = UWorld::GetSubsystem<UMassSignalSubsystem>(World);
			if (SignalSub)
			{
				SignalSub->SignalEntity(UE::ArcMassAbilities::Signals::AbilityStateTreeTickRequired, CapturedEntity);
			}
		}).GetHandle();

	return EStateTreeRunStatus::Running;
}

void FArcAbilityTask_WaitEnhancedInputGated::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	FMassEntityHandle Entity = AbilityContext.GetEntity();

	APawn* Pawn = ArcAbilityTask_WaitEnhancedInputGatedInternal::ResolvePawn(EntityManager, Entity);
	if (Pawn)
	{
		UEnhancedInputComponent* ArcIC = Pawn->FindComponentByClass<UEnhancedInputComponent>();
		if (ArcIC)
		{
			ArcIC->RemoveBindingByHandle(InstanceData.PressedHandle);
			ArcIC->RemoveBindingByHandle(InstanceData.TriggeredHandle);
			ArcIC->RemoveBindingByHandle(InstanceData.ReleasedHandle);
		}
	}

	if (InstanceData.InputMappingContext)
	{
		if (Pawn)
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
	}

	InstanceData.PressedHandle = INDEX_NONE;
	InstanceData.TriggeredHandle = INDEX_NONE;
	InstanceData.ReleasedHandle = INDEX_NONE;
}
