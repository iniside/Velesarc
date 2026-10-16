/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Pawn/ArcMassPlayerInputComponent.h"

#include "ArcMassGameplayFramework.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Input/ArcMassCoreInputComponent.h"
#include "Input/ArcMassInputActionConfig.h"
#include "Pawn/ArcMassFrameworkAgentComponent.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

namespace ArcMassPlayerInput
{
	enum class EArcMassCustomInputPhase : uint8
	{
		Started,
		Triggered,
		Ongoing,
		Canceled,
		Completed
	};

	static void DispatchCustomInput(const TArray<FInstancedStruct>& Actions,
		APawn* Pawn,
		const FInputActionValue& Value,
		FMassEntityHandle Entity,
		FGameplayTag InputTag,
		EArcMassCustomInputPhase Phase)
	{
		for (const FInstancedStruct& Entry : Actions)
		{
			const FArcMassCustomInputAction* Action = Entry.GetPtr<FArcMassCustomInputAction>();
			if (Action == nullptr || Action->InputTag != InputTag)
			{
				continue;
			}

			switch (Phase)
			{
				case EArcMassCustomInputPhase::Started:   Action->OnStarted  (Pawn, Value, Entity); break;
				case EArcMassCustomInputPhase::Triggered: Action->OnTriggered(Pawn, Value, Entity); break;
				case EArcMassCustomInputPhase::Ongoing:   Action->OnOngoing  (Pawn, Value, Entity); break;
				case EArcMassCustomInputPhase::Canceled:  Action->OnCanceled (Pawn, Value, Entity); break;
				case EArcMassCustomInputPhase::Completed: Action->OnCompleted(Pawn, Value, Entity); break;
			}
		}
	}

	// Native input tags. Names match Arcx::Input from ArcCore so existing input data works.
	static FGameplayTag GetInputTagMove()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Move")), /*ErrorIfNotFound*/ false);
		return Tag;
	}

	static FGameplayTag GetInputTagLookMouse()
	{
		static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Look.Mouse")), /*ErrorIfNotFound*/ false);
		return Tag;
	}

	static FMassEntityManager* GetEntityManager(const UWorld* World)
	{
		if (World == nullptr)
		{
			return nullptr;
		}

		UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
		if (MassSubsystem == nullptr)
		{
			return nullptr;
		}

		return &MassSubsystem->GetMutableEntityManager();
	}

	static FMassEntityHandle GetPawnEntityHandle(const APawn* Pawn)
	{
		if (Pawn == nullptr)
		{
			return FMassEntityHandle();
		}

		const UArcMassFrameworkAgentComponent* AgentComp =
			Pawn->FindComponentByClass<UArcMassFrameworkAgentComponent>();
		if (AgentComp == nullptr)
		{
			return FMassEntityHandle();
		}

		return AgentComp->GetEntityHandle();
	}
}

UArcMassPlayerInputComponent::UArcMassPlayerInputComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;
}

void UArcMassPlayerInputComponent::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (CustomInputActions.Num() > 0)
	{
		const APawn* Pawn = GetPawn<APawn>();
		if (Pawn != nullptr)
		{
			if (UArcMassCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcMassCoreInputComponent>())
			{
				for (const FInstancedStruct& Entry : CustomInputActions)
				{
					const FArcMassCustomInputAction* Action = Entry.GetPtr<FArcMassCustomInputAction>();
					if (Action != nullptr && Action->InputTag.IsValid())
					{
						ArcIC->RemoveCustomActions(Action->InputTag);
					}
				}
			}
		}
		CustomInputActions.Reset();
	}

	if (BindHandles.Num() > 0)
	{
		const APawn* Pawn = GetPawn<APawn>();
		if (Pawn)
		{
			if (UArcMassCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcMassCoreInputComponent>())
			{
				ArcIC->RemoveBinds(BindHandles);
			}
		}
		BindHandles.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UArcMassPlayerInputComponent::InitializePlayerInput(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	const APawn* Pawn = GetPawn<APawn>();
	if (Pawn == nullptr)
	{
		return;
	}

	const APlayerController* PC = GetController<APlayerController>();
	if (PC == nullptr)
	{
		return;
	}

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	if (LP == nullptr)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	UArcMassPawnExtensionComponent* PawnExt = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(Pawn);
	if (PawnExt == nullptr)
	{
		return;
	}

	const UArcMassPawnData* PawnData = PawnExt->GetPawnData<UArcMassPawnData>();
	if (PawnData == nullptr)
	{
		return;
	}

	UArcMassCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcMassCoreInputComponent>();
	if (ArcIC == nullptr)
	{
		UE_LOGF(LogArcMassGameFramework, Error
			,"[UArcMassPlayerInputComponent::InitializePlayerInput] Pawn [%ls] has no UArcMassCoreInputComponent.", *GetNameSafe(Pawn));
		return;
	}

	for (UArcMassInputActionConfig* InputConfig : PawnData->InputConfig)
	{
		if (InputConfig == nullptr)
		{
			continue;
		}

		for (const FArcMassInputMappingContextAndPriority& Mapping : InputConfig->InputMappings)
		{
			UInputMappingContext* IMC = Mapping.InputMapping.LoadSynchronous();
			if (IMC == nullptr)
			{
				continue;
			}

			if (Mapping.bRegisterWithSettings)
			{
				if (UEnhancedInputUserSettings* InputSettings = Subsystem->GetUserSettings())
				{
					InputSettings->RegisterInputMappingContext(IMC);
				}
			}

			FModifyContextOptions Options{};
			Options.bIgnoreAllPressedKeysUntilRelease = false;
			Subsystem->AddMappingContext(IMC, Mapping.Priority, Options);
		}

		ArcIC->BindNativeAction(InputConfig, ArcMassPlayerInput::GetInputTagMove(), ETriggerEvent::Triggered, this, &ThisClass::Input_Move, /*bLogIfNotFound*/ true);
		ArcIC->BindNativeAction(InputConfig, ArcMassPlayerInput::GetInputTagMove(), ETriggerEvent::Completed, this, &ThisClass::Input_Move_Completed, /*bLogIfNotFound*/ true);

		ArcIC->BindNativeAction(InputConfig, ArcMassPlayerInput::GetInputTagLookMouse(), ETriggerEvent::Triggered, this, &ThisClass::Input_LookMouse, /*bLogIfNotFound*/ false);

		ArcIC->BindAbilityActions(InputConfig
			, this
			, &ThisClass::Input_AbilityInputTagPressed
			, &ThisClass::Input_AbilityInputTagReleased
			, /*out*/ BindHandles);

		// Copy custom input action structs onto the pawn-scoped component (deep-copy
		// via FInstancedStruct value semantics) so each pawn owns its instances, then
		// bind all five trigger phases for every entry.
		const int32 FirstNewIndex = CustomInputActions.Num();
		CustomInputActions.Append(InputConfig->CustomInputActions);

		for (int32 Idx = FirstNewIndex; Idx < CustomInputActions.Num(); ++Idx)
		{
			const FArcMassCustomInputAction* Action = CustomInputActions[Idx].GetPtr<FArcMassCustomInputAction>();
			if (Action == nullptr)
			{
				continue;
			}
			if (Action->InputAction == nullptr || !Action->InputTag.IsValid())
			{
				continue;
			}

			// BindCustomAction takes UInputAction* (non-const). The struct holds
			// TObjectPtr<const UInputAction>. Enhanced Input does not mutate the
			// action object, so the cast is safe.
			UInputAction* MutableAction = const_cast<UInputAction*>(Action->InputAction.Get());
			ArcIC->BindCustomAction(MutableAction, Action->InputTag, ETriggerEvent::Started,   this, &ThisClass::Input_Custom_Started,   Action->InputTag);
			ArcIC->BindCustomAction(MutableAction, Action->InputTag, ETriggerEvent::Triggered, this, &ThisClass::Input_Custom_Triggered, Action->InputTag);
			ArcIC->BindCustomAction(MutableAction, Action->InputTag, ETriggerEvent::Ongoing,   this, &ThisClass::Input_Custom_Ongoing,   Action->InputTag);
			ArcIC->BindCustomAction(MutableAction, Action->InputTag, ETriggerEvent::Canceled,  this, &ThisClass::Input_Custom_Canceled,  Action->InputTag);
			ArcIC->BindCustomAction(MutableAction, Action->InputTag, ETriggerEvent::Completed, this, &ThisClass::Input_Custom_Completed, Action->InputTag);
		}
	}

	bReadyToBindInputs = true;
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APlayerController*>(PC), UArcMassPawnExtensionComponent::NAME_BindInputsNow);
	UGameFrameworkComponentManager::SendGameFrameworkComponentExtensionEvent(const_cast<APawn*>(Pawn), UArcMassPawnExtensionComponent::NAME_BindInputsNow);
}

void UArcMassPlayerInputComponent::Input_Move(const FInputActionValue& InputActionValue)
{
	const FVector2D Value = InputActionValue.Get<FVector2D>();
	MoveInputVector.X = Value.X;
	MoveInputVector.Y = Value.Y;
}

void UArcMassPlayerInputComponent::Input_Move_Completed(const FInputActionValue& InputActionValue)
{
	MoveInputVector.X = 0;
	MoveInputVector.Y = 0;
}

void UArcMassPlayerInputComponent::Input_LookMouse(const FInputActionValue& InputActionValue)
{
	APawn* Pawn = GetPawn<APawn>();
	if (Pawn == nullptr)
	{
		return;
	}

	const FVector2D Value = InputActionValue.Get<FVector2D>();

	if (Value.X != 0.0f)
	{
		Pawn->AddControllerYawInput(Value.X);
	}

	if (Value.Y != 0.0f)
	{
		Pawn->AddControllerPitchInput(Value.Y);
	}
}

void UArcMassPlayerInputComponent::Input_AbilityInputTagPressed(FGameplayTag InputTag)
{
	const APawn* Pawn = GetPawn<APawn>();
	const FMassEntityHandle EntityHandle = ArcMassPlayerInput::GetPawnEntityHandle(Pawn);
	if (!EntityHandle.IsValid())
	{
		UE_LOGF(LogArcMassGameFramework, Verbose
			,"[UArcMassPlayerInputComponent::Input_AbilityInputTagPressed] No valid Mass entity on pawn [%ls] for tag [%ls].", *GetNameSafe(Pawn)
			, *InputTag.ToString());
		return;
	}

	FMassEntityManager* EntityManager = ArcMassPlayerInput::GetEntityManager(GetWorld());
	if (EntityManager == nullptr)
	{
		return;
	}

	ArcAbilities::PushInput(*EntityManager, EntityHandle, InputTag);
}

void UArcMassPlayerInputComponent::Input_AbilityInputTagReleased(FGameplayTag InputTag)
{
	const APawn* Pawn = GetPawn<APawn>();
	const FMassEntityHandle EntityHandle = ArcMassPlayerInput::GetPawnEntityHandle(Pawn);
	if (!EntityHandle.IsValid())
	{
		UE_LOGF(LogArcMassGameFramework, Verbose
			,"[UArcMassPlayerInputComponent::Input_AbilityInputTagReleased] No valid Mass entity on pawn [%ls] for tag [%ls].", *GetNameSafe(Pawn)
			, *InputTag.ToString());
		return;
	}

	FMassEntityManager* EntityManager = ArcMassPlayerInput::GetEntityManager(GetWorld());
	if (EntityManager == nullptr)
	{
		return;
	}

	ArcAbilities::ReleaseInput(*EntityManager, EntityHandle, InputTag);
}

void UArcMassPlayerInputComponent::Input_Custom_Started(const FInputActionValue& InputActionValue, FGameplayTag InputTag)
{
	APawn* Pawn = GetPawn<APawn>();
	const FMassEntityHandle EntityHandle = ArcMassPlayerInput::GetPawnEntityHandle(Pawn);
	ArcMassPlayerInput::DispatchCustomInput(CustomInputActions, Pawn, InputActionValue, EntityHandle, InputTag, ArcMassPlayerInput::EArcMassCustomInputPhase::Started);
}

void UArcMassPlayerInputComponent::Input_Custom_Triggered(const FInputActionValue& InputActionValue, FGameplayTag InputTag)
{
	APawn* Pawn = GetPawn<APawn>();
	const FMassEntityHandle EntityHandle = ArcMassPlayerInput::GetPawnEntityHandle(Pawn);
	ArcMassPlayerInput::DispatchCustomInput(CustomInputActions, Pawn, InputActionValue, EntityHandle, InputTag, ArcMassPlayerInput::EArcMassCustomInputPhase::Triggered);
}

void UArcMassPlayerInputComponent::Input_Custom_Ongoing(const FInputActionValue& InputActionValue, FGameplayTag InputTag)
{
	APawn* Pawn = GetPawn<APawn>();
	const FMassEntityHandle EntityHandle = ArcMassPlayerInput::GetPawnEntityHandle(Pawn);
	ArcMassPlayerInput::DispatchCustomInput(CustomInputActions, Pawn, InputActionValue, EntityHandle, InputTag, ArcMassPlayerInput::EArcMassCustomInputPhase::Ongoing);
}

void UArcMassPlayerInputComponent::Input_Custom_Canceled(const FInputActionValue& InputActionValue, FGameplayTag InputTag)
{
	APawn* Pawn = GetPawn<APawn>();
	const FMassEntityHandle EntityHandle = ArcMassPlayerInput::GetPawnEntityHandle(Pawn);
	ArcMassPlayerInput::DispatchCustomInput(CustomInputActions, Pawn, InputActionValue, EntityHandle, InputTag, ArcMassPlayerInput::EArcMassCustomInputPhase::Canceled);
}

void UArcMassPlayerInputComponent::Input_Custom_Completed(const FInputActionValue& InputActionValue, FGameplayTag InputTag)
{
	APawn* Pawn = GetPawn<APawn>();
	const FMassEntityHandle EntityHandle = ArcMassPlayerInput::GetPawnEntityHandle(Pawn);
	ArcMassPlayerInput::DispatchCustomInput(CustomInputActions, Pawn, InputActionValue, EntityHandle, InputTag, ArcMassPlayerInput::EArcMassCustomInputPhase::Completed);
}
