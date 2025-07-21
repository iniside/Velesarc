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

#include "ArcCore/AbilitySystem/Tasks/Input/ArcAT_WaitEnhancedInputAction.h"
#include "ArcCore/AbilitySystem/ArcCoreAbilitySystemComponent.h"

#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "Input/ArcCoreInputComponent.h"

UArcAT_WaitEnhancedInputAction* UArcAT_WaitEnhancedInputAction::WaitEnhancedInputAction(UGameplayAbility* OwningAbility
																						, class UInputMappingContext*
																						InMappingContext
																						, class UInputAction*
																						InInputAction
																						, FName TaskInstanceName)
{
	UArcAT_WaitEnhancedInputAction* MyObj = NewAbilityTask<UArcAT_WaitEnhancedInputAction>(OwningAbility
		, TaskInstanceName);
	MyObj->MappingContext = InMappingContext;
	MyObj->InputAction = InInputAction;

	return MyObj;
}

void UArcAT_WaitEnhancedInputAction::Activate()
{
	Super::Activate();
	if (AbilitySystemComponent->GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetAvatarActor());

	const APlayerController* PC = Pawn->GetController<APlayerController>();
	check(PC);

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);
	FModifyContextOptions Context;
	Context.bForceImmediately = true;
	Context.bIgnoreAllPressedKeysUntilRelease = false;
	Subsystem->AddMappingContext(MappingContext
		, 100
		, Context);

	if(InputAction)
	{
		UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();
		PressedHandle = ArcIC->BindAction(InputAction.Get()
			, ETriggerEvent::Started
			, this
			, &UArcAT_WaitEnhancedInputAction::HandleOnInputPressed
			, InputAction.Get()).GetHandle();
		TriggeredHandle = ArcIC->BindAction(InputAction.Get()
			, ETriggerEvent::Triggered
			, this
			, &UArcAT_WaitEnhancedInputAction::HandleOnInputTriggered
			, InputAction.Get()).GetHandle();
		ReleasedHandle = ArcIC->BindAction(InputAction.Get()
			, ETriggerEvent::Completed
			, this
			, &UArcAT_WaitEnhancedInputAction::HandleOnInputReleased
			, InputAction.Get()).GetHandle();
	}
}

void UArcAT_WaitEnhancedInputAction::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);
	if (AbilitySystemComponent->GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetAvatarActor());

	const APlayerController* PC = Pawn->GetController<APlayerController>();
	if (PC == nullptr)
	{
		return;
	}
	check(PC);

	const ULocalPlayer* LP = PC->GetLocalPlayer();
	check(LP);
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	check(Subsystem);

	if (InputAction)
	{
		UArcCoreInputComponent* ArcIC = Pawn->FindComponentByClass<UArcCoreInputComponent>();
		ArcIC->RemoveBindingByHandle(PressedHandle);
		ArcIC->RemoveBindingByHandle(TriggeredHandle);
		ArcIC->RemoveBindingByHandle(ReleasedHandle);
	}
	
	FModifyContextOptions Context;
	Context.bForceImmediately = true;
	Context.bIgnoreAllPressedKeysUntilRelease = false;
	Subsystem->RemoveMappingContext(MappingContext
		, Context);
}

void UArcAT_WaitEnhancedInputAction::HandleOnInputPressed(UInputAction* InInputAction)
{
	OnInputPressed.Broadcast(0.0f);
}

void UArcAT_WaitEnhancedInputAction::HandleOnInputTriggered(UInputAction* InInputAction)
{
	OnInputTriggered.Broadcast(0.0f);
}

void UArcAT_WaitEnhancedInputAction::HandleOnInputReleased(UInputAction* InInputAction)
{
	OnInputReleased.Broadcast(0.0f);
}
