/**
 * This file is part of ArcX.
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

#include "GameMode/ArcCoreGameMode.h"

#include "GameMode/ArcCoreGameSession.h"
#include "GameMode/ArcCoreGameState.h"
#include "Player/ArcCorePlayerController.h"
#include "Core/ArcCoreAssetManager.h"
// #include "ArcCore/GameFramework/ArcPlayerBotController.h"
#include "Player/ArcCoreCharacter.h"
#include "Player/ArcCorePlayerState.h"
// #include "UI/ArcHUD.h"
#include "ArcCoreGameInstance.h"
#include "CommonSessionSubsystem.h"
#include "CommonUserSubsystem.h"
#include "GameMapsSettings.h"
#include "ArcCore/Development/ArcDeveloperSettings.h"
#include "GameMode/ArcExperienceDefinition.h"
#include "GameMode/ArcExperienceManagerComponent.h"
#include "GameMode/ArcCoreWorldSettings.h"
#include "Pawn/ArcPawnData.h"
#include "Pawn/ArcPawnExtensionComponent.h"
#include "Player/ArcPlayerSpawningManagerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "GameMode/ArcExperienceData.h"
#include "GameMode/ArcUserFacingExperienceDefinition.h"

DEFINE_LOG_CATEGORY(LogArcGameModeBase);

AArcCoreGameMode::AArcCoreGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AArcCoreGameState::StaticClass();
	GameSessionClass = AArcCoreGameSession::StaticClass();
	PlayerControllerClass = AArcCorePlayerController::StaticClass();
	// ReplaySpectatorPlayerControllerClass = AArcReplayPlayerController::StaticClass();
	PlayerStateClass = AArcCorePlayerState::StaticClass();
	DefaultPawnClass = AArcCoreCharacter::StaticClass();
	// HUDClass = AArcHUD::StaticClass();
	bUseSeamlessTravel = false;
}

void AArcCoreGameMode::PreInitializeComponents()
{
	UArcExperienceDefinition* ExperienceDefinition = GetCurrentExperienceDefinition();
	if (ExperienceDefinition == nullptr)
	{
		Super::PreInitializeComponents();
		return;
	}

	UArcCoreGameInstance* GameInstance = GetGameInstance<UArcCoreGameInstance>();
	
	//TSoftObjectPtr<UArcExperienceDefinition> SoftCurrentExperience = ExperienceDefinition;
	//GameInstance->SetCurrentExperience(SoftCurrentExperience);
	
	UArcCoreAssetManager::Get().SetExperienceBundlesAsync(GetWorld(), ExperienceDefinition->BundlesState.GetBundles(GetNetMode()));
	
	UClass* CurrentGameStateClass = GameStateClass; 
	GameStateClass = ExperienceDefinition->GameStateClass.LoadSynchronous();

	if (GameStateClass == nullptr)
	{
		GameStateClass = CurrentGameStateClass;
	}
	
	Super::PreInitializeComponents();
}

const UArcPawnData* AArcCoreGameMode::GetPawnDataForController(const AController* InController) const
{
	// See if pawn data is already set on the player state
	if (InController != nullptr)
	{
		if (const AArcCorePlayerState* ArcPS = InController->GetPlayerState<AArcCorePlayerState>())
		{
			if (const UArcPawnData* PawnData = ArcPS->GetPawnData<UArcPawnData>())
			{
				return PawnData;
			}
		}
	}

	// If not, fall back to the the default for the current experience
	check(GameState);
	UArcExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcExperienceManagerComponent>();
	check(ExperienceComponent);

	if (ExperienceComponent->IsExperienceLoaded())
	{
		const UArcExperienceDefinition* Experience = ExperienceComponent->GetCurrentExperienceChecked();
		if (Experience->GetDefaultPawnData().IsNull() == false)
		{
			return Experience->GetDefaultPawnData().LoadSynchronous();
		}

		// Experience is loaded and there's still no pawn data, fall back to the default
		// for now
		//@TODO: Remove entirely?
		return UArcCoreAssetManager::Get().GetDefaultPawnData();
	}

	// Experience not loaded yet, so there is no pawn data to be had
	return nullptr;
}

void AArcCoreGameMode::InitGame(const FString& MapName
								, const FString& Options
								, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Wait for the next frame to give time to initialize startup settings
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

UClass* AArcCoreGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const UArcPawnData* PawnData = GetPawnDataForController(InController))
	{
		if (PawnData->GetPawnClass().IsNull() == false)
		{
			return PawnData->GetPawnClass().LoadSynchronous();
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

APawn* AArcCoreGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer
																	, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient; // Never save the default player pawns into a map.
	SpawnInfo.bDeferConstruction = true;

	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass
			, SpawnTransform
			, SpawnInfo))
		{
			SpawnedPawn->FinishSpawning(SpawnTransform);

			if (UArcPawnExtensionComponent* PawnExtComp = UArcPawnExtensionComponent::FindPawnExtensionComponent(
				SpawnedPawn))
			{
				if (const UArcPawnData* PawnData = GetPawnDataForController(NewPlayer))
				{
					PawnExtComp->SetPawnData(PawnData);
				}
				else
				{
					UE_LOG(LogArcGameModeBase
						, Error
						, TEXT( "Game mode was unable to set PawnData on the spawned pawn [%s].")
						, *GetNameSafe(SpawnedPawn));
				}
			}

			return SpawnedPawn;
		}
		else
		{
			UE_LOG(LogArcGameModeBase
				, Log
				, TEXT("Game mode was unable to spawn Pawn of class [%s] at [%s].")
				, *GetNameSafe(PawnClass)
				, *SpawnTransform.ToHumanReadableString());
		}
	}
	else
	{
		UE_LOG(LogArcGameModeBase
			, Log
			, TEXT("Game mode was unable to spawn Pawn due to NULL pawn class."));
	}

	return nullptr;
}


bool AArcCoreGameMode::ShouldSpawnAtStartSpot(AController* Player)
{
	// We never want to use the start spot, always use the spawn management component.
	return false;
}

void AArcCoreGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Delay starting new players until the experience has been loaded
	// (players who log in prior to that will be started by OnExperienceLoaded)
	if (IsExperienceLoaded())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
}

AActor* AArcCoreGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (UArcPlayerSpawningManagerComponent* PlayerSpawningComponent = GameState->FindComponentByClass<
		UArcPlayerSpawningManagerComponent>())
	{
		return PlayerSpawningComponent->ChoosePlayerStart(Player);
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

void AArcCoreGameMode::FinishRestartPlayer(AController* NewPlayer
										   , const FRotator& StartRotation)
{
	if (UArcPlayerSpawningManagerComponent* PlayerSpawningComponent = GameState->FindComponentByClass<
		UArcPlayerSpawningManagerComponent>())
	{
		PlayerSpawningComponent->FinishRestartPlayer(NewPlayer
			, StartRotation);
	}

	Super::FinishRestartPlayer(NewPlayer
		, StartRotation);
}

bool AArcCoreGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	if (UArcPlayerSpawningManagerComponent* PlayerSpawningComponent = GameState->FindComponentByClass<
		UArcPlayerSpawningManagerComponent>())
	{
		return Super::PlayerCanRestart_Implementation(Player) && PlayerSpawningComponent->PlayerCanRestart(Player);
	}

	return Super::PlayerCanRestart_Implementation(Player);
}

void AArcCoreGameMode::InitGameState()
{
	Super::InitGameState();
	
	UArcExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcExperienceManagerComponent>();
	check(ExperienceComponent);
	
	// Listen for the experience load to complete
	ExperienceComponent->CallOrRegister_OnExperienceLoaded(FOnArcExperienceLoaded::FDelegate::CreateUObject(this
		, &ThisClass::OnExperienceLoaded));
}

bool AArcCoreGameMode::UpdatePlayerStartSpot(AController* Player
											 , const FString& Portal
											 , FString& OutErrorMessage)
{
	// Do nothing, we'll wait until PostLogin when we try to spawn the player for real.
	// Doing anything right now is no good, systems like team assignment haven't even
	// occurred yet.
	return true;
}

void AArcCoreGameMode::GenericPlayerInitialization(AController* NewPlayer)
{
	Super::GenericPlayerInitialization(NewPlayer);
}

void AArcCoreGameMode::FailedToRestartPlayer(AController* NewPlayer)
{
	Super::FailedToRestartPlayer(NewPlayer);

	// If we tried to spawn a pawn and it failed, lets try again *note* check if there's actually a pawn class
	// before we try this forever.
	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		if (APlayerController* NewPC = Cast<APlayerController>(NewPlayer))
		{
			// If it's a player don't loop forever, maybe something changed and they can no longer restart if so stop trying.
			if (PlayerCanRestart(NewPC))
			{
				RequestPlayerRestartNextFrame(NewPlayer, false);			
			}
			else
			{
				UE_LOG(LogArcGameModeBase, Verbose, TEXT("FailedToRestartPlayer(%s) and PlayerCanRestart returned false, so we're not going to try again."), *GetPathNameSafe(NewPlayer));
			}
		}
		else
		{
			RequestPlayerRestartNextFrame(NewPlayer, false);
		}
	}
	else
	{
		UE_LOG(LogArcGameModeBase, Verbose, TEXT("FailedToRestartPlayer(%s) but there's no pawn class so giving up."), *GetPathNameSafe(NewPlayer));
	}
}

APlayerController* AArcCoreGameMode::SpawnPlayerController(ENetRole InRemoteRole
														   , const FString& Options)
{
	UArcExperienceDefinition* ExperienceDefinition = GetCurrentExperienceDefinition();
	
	if (ExperienceDefinition)
	{
		TSubclassOf<AArcCorePlayerController> PCClass = ExperienceDefinition->PlayerControllerClass.LoadSynchronous();
		if (PCClass)
		{
			return SpawnPlayerControllerCommon(InRemoteRole, FVector::ZeroVector, FRotator::ZeroRotator, PCClass);;
		}
	}
	
	UE_LOG(LogArcGameModeBase, Error, TEXT("Experience not loaded. Using Default Player Controller %s"), *GetNameSafe(PlayerControllerClass))
	return Super::SpawnPlayerController(InRemoteRole, Options);
}

void AArcCoreGameMode::OnPostLogin(AController* NewPlayer)
{
	Super::OnPostLogin(NewPlayer);

	OnGameModeCombinedPostLoginDelegate.Broadcast(this, NewPlayer);
}

void AArcCoreGameMode::HandleMatchAssignmentIfNotExpectingOne()
{
	FString ExperienceIdSource;
	FPrimaryAssetId ExperienceId = GetExperiencePrimaryId(ExperienceIdSource);

	OnMatchAssignmentGiven(ExperienceId, ExperienceIdSource);
}

FPrimaryAssetId AArcCoreGameMode::GetExperiencePrimaryId(FString& OutExperienceIdSource) const
{
	FPrimaryAssetId ExperienceId;
	FString ExperienceIdSource;

	// Precedence order (highest wins)
	//  - URL Options override
	//  - Matchmaking assignment (if present) (UserExperience)
	//  - Command Line override
	//  - World Settings
	//  - Developer Settings (PIE only)

	UWorld* World = GetWorld();
	
	if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
		ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UArcExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromOptions));
		ExperienceIdSource = TEXT("OptionsString");
	}

	// "UserExperience="
	if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("UserExperience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("UserExperience"));
		UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
		FPrimaryAssetId UserExperienceId = FPrimaryAssetId::FromString(ExperienceFromOptions);
		
		TSharedPtr<FStreamableHandle> Handle = AssetManager.LoadPrimaryAsset(UserExperienceId);
		if (ensure(Handle.IsValid()))
		{
			Handle->WaitUntilComplete();
		}
		UArcUserFacingExperienceDefinition* UserFacing = Cast<UArcUserFacingExperienceDefinition>(Handle->GetLoadedAsset());
		ExperienceIdSource = "User Facing Experience";
		ExperienceId = UserFacing->ExperienceID;
		
	}
	
	// see if the command line wants to set the experience
	if (!ExperienceId.IsValid())
	{
		FString ExperienceFromCommandLine;
		if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), ExperienceFromCommandLine))
		{
			ExperienceId = FPrimaryAssetId::ParseTypeAndName(ExperienceFromCommandLine);
			if (!ExperienceId.PrimaryAssetType.IsValid())
			{
				ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UArcExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromCommandLine));
			}
			ExperienceIdSource = TEXT("CommandLine");
		}
	}

	// see if the world settings has a default experience
	if (!ExperienceId.IsValid())
	{
		if (AArcCoreWorldSettings* TypedWorldSettings = Cast<AArcCoreWorldSettings>(GetWorldSettings()))
		{
			ExperienceId = TypedWorldSettings->GetDefaultExperience();
			ExperienceIdSource = TEXT("WorldSettings");
		}
	}
	
#if WITH_EDITOR
	if (!ExperienceId.IsValid() && World->IsPlayInEditor())
	{
		ExperienceId = GetDefault<UArcDeveloperSettings>()->ExperienceOverride;
		ExperienceIdSource = TEXT("DeveloperSettings");
	}
#endif
	
	OutExperienceIdSource = ExperienceIdSource; 
	return ExperienceId;
}

UArcExperienceDefinition* AArcCoreGameMode::GetCurrentExperienceDefinition() const
{
	FString ExperienceIdSource;
	FPrimaryAssetId ExperienceId = GetExperiencePrimaryId(ExperienceIdSource);
	
	UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
	
	UArcExperienceDefinition* def = UArcCoreAssetManager::GetAsset<UArcExperienceDefinition>(ExperienceId, false);
	FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(ExperienceId);
	UArcExperienceDefinition* Experience = def;//Cast<UArcExperienceDefinition>(AssetPath.TryLoad());

	return Experience;
}

void AArcCoreGameMode::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId
											  , const FString& ExperienceIdSource)
{
	if (ExperienceId.IsValid())
	{
		UE_LOG(LogArcGameModeBase
			, Log
			, TEXT("Identified experience %s (Source: %s)")
			, *ExperienceId.ToString()
			, *ExperienceIdSource);

		UArcExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcExperienceManagerComponent>();
		check(ExperienceComponent);
		ExperienceComponent->ServerSetCurrentExperience(ExperienceId);
	}
	else
	{
		FString DefaultMap = UGameMapsSettings::GetGameDefaultMap();
		UWorld* World = GetWorld();
		UGameInstance* GameInstance = GetGameInstance();
		
		if (GameInstance && World && World->GetNetMode() == NM_DedicatedServer && World->URL.Map == DefaultMap)
		{
			UE_LOG(LogArcGameModeBase
			, Log
			, TEXT("Loading to default map with not experience. Map %s"), *DefaultMap);
		}
		else
		{
			UE_LOG(LogArcGameModeBase
				, Error
				, TEXT("Failed to identify experience, loading screen will stay up forever"));	
		}
		
	}
}

void AArcCoreGameMode::OnExperienceLoaded(const UArcExperienceDefinition* CurrentExperience)
{
	// Spawn any players that are already attached
	//@TODO: Here we're handling only *player* controllers, but in
	// GetDefaultPawnClassForController_Implementation we
	// skipped all controllers
	// GetDefaultPawnClassForController_Implementation might only be getting called for
	// players anyways
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PC = Cast<APlayerController>(*Iterator);
		if ((PC != nullptr) && (PC->GetPawn() == nullptr))
		{
			if (PlayerCanRestart(PC))
			{
				RestartPlayer(PC);
			}
		}
	}
}

bool AArcCoreGameMode::IsExperienceLoaded() const
{
	check(GameState);
	UArcExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcExperienceManagerComponent>();
	check(ExperienceComponent);

	return ExperienceComponent->IsExperienceLoaded();
}

void AArcCoreGameMode::RequestPlayerRestartNextFrame(AController* Controller
													 , bool bForceReset)
{
	if (bForceReset && (Controller != nullptr))
	{
		Controller->Reset();
	}

	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		GetWorldTimerManager().SetTimerForNextTick(PC, &APlayerController::ServerRestartPlayer_Implementation);
	}
}
