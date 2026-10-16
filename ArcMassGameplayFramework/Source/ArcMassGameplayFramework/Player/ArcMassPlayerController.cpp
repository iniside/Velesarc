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

#include "Player/ArcMassPlayerController.h"

#include "Player/ArcMassPlayerControllerExtensionComponent.h"

#include "AsyncMessageBindingComponent.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameplayControlRotationComponent.h"
#include "GameFramework/IGameplayCameraSystemHost.h"

#include "Commands/ArcReplicatedCommand.h"
#include "Commands/ArcReplicatedCommandDispatch.h"

#include "Experience/ArcMassExperienceDefinition.h"
#include "Experience/ArcMassExperienceManagerComponent.h"
#include "GameMode/ArcMassGameMode.h"
#include "Player/ArcMassPlayerState.h"
#include "GameFramework/GameplayCamerasPlayerCameraManager.h"

DEFINE_LOG_CATEGORY(LogArcMassPlayerController);

IGameplayCameraSystemHost* AArcMassPlayerController::GetGameplayCameraSystemHost()
{
	return GameplayCameraSystemHost;
}

AArcMassPlayerController::AArcMassPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;

	PlayerCameraManagerClass = AGameplayCamerasPlayerCameraManager::StaticClass();

	MyPlayerControllerExtensionComponent = CreateDefaultSubobject<UArcMassPlayerControllerExtensionComponent>(TEXT("PlayerControllerExtension"));

	AsyncMessageBindingComponent = CreateDefaultSubobject<UAsyncMessageBindingComponent>(TEXT("AsyncMessageBinding"));
	// The engine component builds its endpoint in InitializeComponent(), which only fires when this flag is
	// set (the component never sets it itself). Without this the endpoint stays null and scoped broadcasts
	// silently fall back to the world-wide endpoint.
	AsyncMessageBindingComponent->bWantsInitializeComponent = true;
}

void AArcMassPlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	GameplayCameraSystemHost = IGameplayCameraSystemHost::FindActiveHost(this);
	CacheControlRotationComp();
}

void AArcMassPlayerController::BeginPlay()
{
	Super::BeginPlay();
	GameplayCameraSystemHost = IGameplayCameraSystemHost::FindActiveHost(this);
	CacheControlRotationComp();
}

void AArcMassPlayerController::CacheControlRotationComp()
{
	if (ControlRotationComp != nullptr)
	{
		return;
	}

	ControlRotationComp = FindComponentByClass<UGameplayControlRotationComponent>();
	if (ControlRotationComp != nullptr)
	{
		// We drive TickComponent manually from TickActor (after Super::TickActor runs
		// UpdateRotation) to preserve ordering. Disabling auto-tick avoids a double-tick
		// if the component opted into engine ticking.
		ControlRotationComp->PrimaryComponentTick.bCanEverTick = false;
		ControlRotationComp->PrimaryComponentTick.SetTickFunctionEnable(false);
	}
}

void AArcMassPlayerController::TickActor(float DeltaSeconds
	, ELevelTick TickType
	, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaSeconds, TickType, ThisTickFunction);

	// Drive the gameplay-camera-to-control-rotation bridge after Super::TickActor has
	// run UpdateRotation, so camera-driven yaw/pitch is folded into ControlRotation.
	if (ControlRotationComp != nullptr)
	{
		ControlRotationComp->TickComponent(DeltaSeconds, ELevelTick::LEVELTICK_All, &ControlRotationComp->PrimaryComponentTick);
	}
}

void AArcMassPlayerController::AddPitchInput(float Val)
{
	OnPitchInputAddDelegate.Broadcast(Val);
	Super::AddPitchInput(Val);
}

void AArcMassPlayerController::AddYawInput(float Val)
{
	OnYawInputAddDelegate.Broadcast(Val);
	Super::AddYawInput(Val);
}

bool AArcMassPlayerController::ShouldPredict(const FArcReplicatedCommandHandle& Command) const
{
	return !HasAuthority() && Command.ExecutesOnClient();
}

bool AArcMassPlayerController::SendReplicatedCommand(const FArcReplicatedCommandHandle& Command)
{
	if (ShouldPredict(Command))
	{
		if (!Arcx::ReplicatedCommands::PreparePredictedSend(Command, this, FPlatformTime::Seconds(), bPredictionReplaying
			, PredictionSequence, PredictedCommands))
		{
			return false;
		}

		ServerSendReplicatedCommand(Command);
		return true;
	}

	if (Arcx::ReplicatedCommands::PrepareSend(Command))
	{
		ServerSendReplicatedCommand(Command);
		return true;
	}

	return false;
}

bool AArcMassPlayerController::SendReplicatedCommand(const FArcReplicatedCommandHandle& Command
	, FArcCommandConfirmedDelegate&& ConfirmDelegate)
{
	if (ShouldPredict(Command))
	{
		if (!Arcx::ReplicatedCommands::PreparePredictedSendWithConfirm(Command, MoveTemp(ConfirmDelegate), this, FPlatformTime::Seconds()
			, bPredictionReplaying, PredictionSequence, PredictedCommands, CommandResponses))
		{
			return false;
		}

		ServerSendReplicatedCommand(Command);
		return true;
	}

	if (Arcx::ReplicatedCommands::PrepareSendWithConfirm(Command, MoveTemp(ConfirmDelegate), CommandResponses))
	{
		ServerSendReplicatedCommand(Command);
		return true;
	}

	return false;
}

bool AArcMassPlayerController::SendClientReplicatedCommand(const FArcReplicatedCommandHandle& Command)
{
	if (Arcx::ReplicatedCommands::PrepareClientSend(Command))
	{
		ClientSendReplicatedCommand(Command);
		return true;
	}

	return false;
}

void AArcMassPlayerController::ClientSendReplicatedCommand_Implementation(const FArcReplicatedCommandHandle& Command)
{
	Arcx::ReplicatedCommands::HandleClientExecute(Command, this);
}

void AArcMassPlayerController::ClientConfirmReplicatedCommand_Implementation(const FGuid& CommandId, bool bResult)
{
	FArcPredictedCommand Rejected;
	if (Arcx::ReplicatedCommands::HandlePredictionConfirm(CommandId, bResult, this, bPredictionReplaying, PredictedCommands, Rejected))
	{
		UE_LOGF(LogArcMassPlayerController, Warning, "Predicted command %ls rejected by the server: Id=%ls Sequence=%u Age=%.3fs"
			, *Rejected.Command->GetScriptStruct()->GetName()
			, *Rejected.CommandId.ToString()
			, Rejected.PredictionSequence
			, FPlatformTime::Seconds() - Rejected.SendTimeSeconds);
	}

	Arcx::ReplicatedCommands::HandleConfirm(CommandId, bResult, CommandResponses);
}

void AArcMassPlayerController::ServerSendReplicatedCommand_Implementation(
	const FArcReplicatedCommandHandle& Command)
{
	const bool bResult = Command.Execute(this);
	const FGuid& CommandId = Command.GetCommandId();
	ClientConfirmReplicatedCommand(CommandId, bResult);
}
