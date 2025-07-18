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

#include "Player/ArcCorePlayerController.h"

#include "ArcPlayerStateExtensionComponent.h"
#include "AbilitySystem/ArcCoreAbilitySystemComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameState.h"

#include "ArcCore/Camera/ArcPlayerCameraManager.h"
#include "GameMode/ArcCoreGameMode.h"
#include "Player/ArcCorePlayerState.h"
#include "Core/ArcCoreAssetManager.h"

#include "Commands/ArcReplicatedCommand.h"
#include "Components/InputComponent.h"
#include "GameFramework/GameplayControlRotationComponent.h"
#include "GameMode/ArcExperienceManagerComponent.h"

#include "Pawn/ArcPawnExtensionComponent.h"

DEFINE_LOG_CATEGORY(LogArcCorePlayerController);

IGameplayCameraSystemHost* AArcCorePlayerController::GetGameplayCameraSystemHost()
{
	return GameplayCameraSystemHost;
}

bool AArcCorePlayerController::ShouldShowLoadingScreen(FString& OutReason) const
{
	if (GetPlayerState<APlayerState>() == nullptr)
	{
		OutReason = TEXT("Player State is invalid or not replicated");
		return true;
	}

	return false;
}

void AArcCorePlayerController::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if(IGameplayTagAssetInterface* GTI = Cast<IGameplayTagAssetInterface>(GetPawn()))
	{
		GTI->GetOwnedGameplayTags(TagContainer);
		return;
	}
	
	if(AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
	{
		if (ArcPS->GetArcAbilitySystemComponent())
		{
			ArcPS->GetArcAbilitySystemComponent()->GetOwnedGameplayTags(TagContainer);
			return;
		}
	}
}

// Sets default values
AArcCorePlayerController::AArcCorePlayerController()
{
	bShowMouseCursor = true;
	PlayerCameraManagerClass = AArcPlayerCameraManager::StaticClass();
	// Set this actor to call Tick() every frame.  You can turn this off to improve
	// performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

void AArcCorePlayerController::PreInitializeComponents()
{
	Super::PreInitializeComponents();
	GameplayCameraSystemHost = IGameplayCameraSystemHost::FindActiveHost(this);
}

// Called when the game starts or when spawned
void AArcCorePlayerController::BeginPlay()
{
	Super::BeginPlay();
	GameplayCameraSystemHost = IGameplayCameraSystemHost::FindActiveHost(this);
	UGameplayControlRotationComponent* ControlRotationComp = FindComponentByClass<UGameplayControlRotationComponent>();
	
	if (ControlRotationComp)
	{
		ControlRotationComp->PrimaryComponentTick.SetTickFunctionEnable(false);
	}
}

void AArcCorePlayerController::TickActor(float DeltaSeconds
	, ELevelTick TickType
	, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaSeconds
		, TickType
		, ThisTickFunction);

	UGameplayControlRotationComponent* ControlRotationComp = FindComponentByClass<UGameplayControlRotationComponent>();
	if (ControlRotationComp)
	{
		ControlRotationComp->TickComponent(DeltaSeconds, ELevelTick::LEVELTICK_All, &ControlRotationComp->PrimaryComponentTick);
	}
}

void AArcCorePlayerController::PreProcessInput(const float DeltaTime
											   , const bool bGamePaused)
{
	Super::PreProcessInput(DeltaTime, bGamePaused);
}

void AArcCorePlayerController::PostProcessInput(const float DeltaTime
												, const bool bGamePaused)
{
	if (PlayerState)
	{
		if (UArcCoreAbilitySystemComponent* SfxASC = PlayerState->FindComponentByClass<
			UArcCoreAbilitySystemComponent>())
		{
			SfxASC->ProcessAbilityInput(DeltaTime
				, bGamePaused);
		}
	}


	Super::PostProcessInput(DeltaTime
		, bGamePaused);


}

void AArcCorePlayerController::OnRep_Pawn()
{
	Super::OnRep_Pawn();

	if (UArcPawnExtensionComponent* PawnExt = UArcPawnExtensionComponent::FindPawnExtensionComponent(GetPawn()))
	{
		PawnExt->HandlePawnReplicatedFromController();
	}

	if (APlayerState* PS = GetPlayerState<APlayerState>())
	{
		if (UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(PS))
		{
			PSExt->CheckDefaultInitialization();
		}	
	}
	
	
	if(AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
	{
		UE_LOG(LogArcCorePlayerController, Log, TEXT("AArcCorePlayerController::OnRep_Pawn CheckDataReady"))
		ArcPS->CheckDataReady();
	}

	if (APlayerState* PS = GetPlayerState<APlayerState>())
	{
		if (bComponentsInitialized)
		{
			return;
		}
		bComponentsInitialized = true;
		TArray<UArcCorePlayerControllerComponent*> Components;
		GetComponents<UArcCorePlayerControllerComponent>(Components);

		for (UArcCorePlayerControllerComponent* C : Components)
		{
			C->OnPossesed(GetPawn());
		}
	}
}

void AArcCorePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	{
		TArray<UArcCorePlayerControllerComponent*> Components;
		GetComponents<UArcCorePlayerControllerComponent>(Components);

		for (UArcCorePlayerControllerComponent* C : Components)
		{
			C->OnPossesed(GetPawn());
		}
	}
}

void AArcCorePlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	if (UArcPawnExtensionComponent* PawnExt = UArcPawnExtensionComponent::FindPawnExtensionComponent(GetPawn()))
	{
		PawnExt->HandlePlayerStateReplicatedFromController();
	}
	
	if (UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(GetPlayerState<APlayerState>()))
	{
		PSExt->CheckDefaultInitialization();
	}
	
	if(AArcCorePlayerState* ArcPS = GetPlayerState<AArcCorePlayerState>())
	{
		UE_LOG(LogArcCorePlayerController, Log, TEXT("AArcCorePlayerController::OnRep_PlayerState CheckDataReady"))
		ArcPS->CheckDataReady();
	}

	if (APawn* P = GetPawn())
	{
		if (bComponentsInitialized)
		{
			return;
		}
		bComponentsInitialized = true;
		TArray<UArcCorePlayerControllerComponent*> Components;
		GetComponents<UArcCorePlayerControllerComponent>(Components);

		for (UArcCorePlayerControllerComponent* C : Components)
		{
			C->OnPossesed(GetPawn());
		}
	}
}

// Called every frame
void AArcCorePlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (GetLocalRole() == ENetRole::ROLE_Authority)
	{
		// That should make PlayerState working with Spatial Filter. Hey ?
		if (GetPawn() && PlayerState)
		{
			FHitResult Unused;
			PlayerState->K2_SetActorLocation(GetPawn()->GetActorLocation(), false, Unused, true);
		}
	}
}

void AArcCorePlayerController::InitPlayerState()
{
	if (GetNetMode() != NM_Client)
	{
		UWorld* const World = GetWorld();
		const AArcCoreGameMode* GameMode = World ? World->GetAuthGameMode<AArcCoreGameMode>() : nullptr;

		const AGameStateBase* const GameState = World ? World->GetGameState() : nullptr;
		// If the GameMode is null, this might be a network client that's trying to
		// record a replay. Try to use the default game mode in this case so that
		// we can still spawn a PlayerState.
		if (GameMode == nullptr)
		{
			
			GameMode = GameState ? GameState->GetDefaultGameMode<AArcCoreGameMode>() : nullptr;
		}

		if (GameMode != nullptr)
		{
			TSubclassOf<APlayerState> PlayerStateClassToSpawn = nullptr;
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.Owner = this;
			SpawnInfo.Instigator = GetInstigator();
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.ObjectFlags |= RF_Transient; // We never want player states to save into a map
			SpawnInfo.bDeferConstruction = true;

			if (GameMode->GetCurrentExperienceDefinition())
			{
				const UArcExperienceDefinition* ED = GameMode->GetCurrentExperienceDefinition();
				
				if (PlayerStateClassToSpawn == nullptr)
				{
					PlayerStateClassToSpawn = ED->PlayerStateClass.LoadSynchronous();
				}
			}
			
			if (PlayerStateClassToSpawn == nullptr)
			{
				PlayerStateClassToSpawn = GameMode->PlayerStateClass;	
			}
			
			if (PlayerStateClassToSpawn.Get() == nullptr)
			{
				UE_LOG(LogPlayerController, Log, TEXT("AController::InitPlayerState: the PlayerStateClass of game mode %s is null, falling back to "
						"AArcCorePlayerState."), *GameMode->GetName());
				PlayerStateClassToSpawn = AArcCorePlayerState::StaticClass();
			}

			PlayerState = World->SpawnActor<AArcCorePlayerState>(PlayerStateClassToSpawn, SpawnInfo);

			PlayerState->FinishSpawning(FTransform::Identity);

			// force a default player name if necessary
			if (PlayerState && PlayerState->GetPlayerName().IsEmpty())
			{
				// don't call SetPlayerName() as that will broadcast entry messages but
				// the GameMode hasn't had a chance to potentially apply a player/bot name
				// yet

				PlayerState->SetPlayerNameInternal(GameMode->DefaultPlayerName.ToString());
			}
		}

		if (APlayerState* PS = GetPlayerState<APlayerState>())
		{
			if (UArcPlayerStateExtensionComponent* PSExt = UArcPlayerStateExtensionComponent::Get(PS))
			{
				PSExt->CheckDefaultInitialization();
			}	
		}
	}
}

void AArcCorePlayerController::AddPitchInput(float Val)
{
	OnRotationInputAddDelegate.Broadcast(Val);
	Super::AddPitchInput(Val);
}

void AArcCorePlayerController::AddYawInput(float Val)
{
	OnRotationInputAddDelegate.Broadcast(Val);
	Super::AddYawInput(Val);
}

void AArcCorePlayerController::ExecuteCommand()
{
	if(GetLocalRole() < ENetRole::ROLE_Authority)
	{
		ServerExecuteCommand();
	}
}

void AArcCorePlayerController::OpenMap(const FString& MapName)
{
	if(GetLocalRole() < ENetRole::ROLE_Authority)
	{
		ServerOpenMap(MapName);
	}
}

void AArcCorePlayerController::ServerOpenMap_Implementation(const FString& MapName)
{
	GetWorld()->ServerTravel(MapName);
}

void AArcCorePlayerController::ServerExecuteCommand_Implementation()
{
	ENetRole R = GetLocalRole();
}

bool AArcCorePlayerController::SendReplicatedCommand(const FArcReplicatedCommandHandle& Command)
{
	if (Command.CanSendCommand())
	{
		Command.PreSendCommand();
		ServerSendReplicatedCommand(Command);

		return true;
	}

	return false;
}

bool AArcCorePlayerController::SendReplicatedCommand(const FArcReplicatedCommandHandle& Command
	, FArcCommandConfirmedDelegate&& ConfirmDelegate)
{
	if (Command.CanSendCommand())
	{
		CommandResponses.Add(Command.GetCommandId(), MoveTemp(ConfirmDelegate));
		Command.PreSendCommand();
		ServerSendReplicatedCommand(Command);

		return true;
	}

	return false;
}

bool AArcCorePlayerController::SendClientReplicatedCommand(const FArcReplicatedCommandHandle& Command)
{
	if (Command.CanSendCommand())
	{
		Command.PreSendCommand();
		ClientSendReplicatedCommand(Command);
		
		return true;
	}
	
	return false;
}

void AArcCorePlayerController::ClientSendReplicatedCommand_Implementation(const FArcReplicatedCommandHandle& Command)
{
	Command.Execute();
}

void AArcCorePlayerController::ClientConfirmReplicatedCommand_Implementation(const FArcReplicatedCommandId& Command, bool bResult)
{
	if (FArcCommandConfirmedDelegate* Function = CommandResponses.Find(Command))
	{
		Function->ExecuteIfBound(bResult);

		CommandResponses.Remove(Command);
	}
}

void AArcCorePlayerController::ServerSendReplicatedCommand_Implementation(
	const FArcReplicatedCommandHandle& Command)
{
	const bool bResult = Command.Execute();
	const FArcReplicatedCommandId& CommandId = Command.GetCommandId();
	ClientConfirmReplicatedCommand(CommandId, bResult);
}
