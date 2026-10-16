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

#include "GameMode/ArcMassGameMode.h"

#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"

#include "Experience/ArcMassExperienceDefinition.h"
#include "Experience/ArcMassExperienceManagerComponent.h"
#include "GameMode/ArcMassGameState.h"
#include "GameMode/ArcMassWorldSettings.h"
#include "Pawn/ArcMassPawnData.h"
#include "Pawn/ArcMassPawnExtensionComponent.h"
#include "Player/ArcMassPlayerController.h"
#include "Player/ArcMassPlayerState.h"

DEFINE_LOG_CATEGORY(LogArcMassGameMode);

AArcMassGameMode::AArcMassGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AArcMassGameState::StaticClass();
	PlayerControllerClass = AArcMassPlayerController::StaticClass();
	PlayerStateClass = AArcMassPlayerState::StaticClass();
}

const UArcMassPawnData* AArcMassGameMode::GetPawnDataForController(const AController* InController) const
{
	if (InController != nullptr)
	{
		if (const AArcMassPlayerState* MassPS = InController->GetPlayerState<AArcMassPlayerState>())
		{
			if (const UArcMassPawnData* ExistingPawnData = MassPS->GetPawnData<UArcMassPawnData>())
			{
				return ExistingPawnData;
			}
		}
	}

	// Fall back to the default for the current experience.
	if (GameState != nullptr)
	{
		if (UArcMassExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcMassExperienceManagerComponent>())
		{
			if (ExperienceComponent->IsExperienceLoaded())
			{
				const UArcMassExperienceDefinition* Experience = ExperienceComponent->GetCurrentExperienceChecked();
				if (Experience->GetDefaultPawnData().IsNull() == false)
				{
					return Experience->GetDefaultPawnData().LoadSynchronous();
				}
			}
		}
	}

	return nullptr;
}

void AArcMassGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Wait one frame so startup settings have a chance to initialize.
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

void AArcMassGameMode::InitGameState()
{
	Super::InitGameState();

	UArcMassExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcMassExperienceManagerComponent>();
	check(ExperienceComponent);

	// GameMode subscribes here in InitGameState, which runs before any actor's BeginPlay,
	// so it lands ahead of generic listeners without needing a priority bucket.
	ExperienceComponent->CallOrRegister_OnExperienceLoaded(
		FOnArcMassExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

UClass* AArcMassGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const UArcMassPawnData* ResolvedPawnData = GetPawnDataForController(InController))
	{
		if (!ResolvedPawnData->PawnClass.IsNull())
		{
			return ResolvedPawnData->PawnClass.LoadSynchronous();
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

APawn* AArcMassGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.bDeferConstruction = true;

	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		if (APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo))
		{
			SpawnedPawn->FinishSpawning(SpawnTransform);
			// Apply PawnData *before* FinishSpawning so it is visible during BeginPlay and
			// the pawn-extension init state can advance using a populated PawnData pointer.
			if (UArcMassPawnExtensionComponent* PawnExtComp = UArcMassPawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
			{
				if (const UArcMassPawnData* ResolvedPawnData = GetPawnDataForController(NewPlayer))
				{
					PawnExtComp->SetPawnData(ResolvedPawnData);
				}
				else
				{
					UE_LOGF(LogArcMassGameMode
						, Warning
						,"ArcMass game mode spawned pawn [%ls] without PawnData (experience system not yet wired).", *GetNameSafe(SpawnedPawn));
				}
			}

			

			return SpawnedPawn;
		}

		UE_LOGF(LogArcMassGameMode
			, Log
			,"ArcMass game mode was unable to spawn Pawn of class [%ls] at [%ls].", *GetNameSafe(PawnClass)
			, *SpawnTransform.ToHumanReadableString());
	}
	else
	{
		UE_LOGF(LogArcMassGameMode, Log,"ArcMass game mode was unable to spawn Pawn due to NULL pawn class.");
	}

	return nullptr;
}

void AArcMassGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// If the experience is not yet loaded, OnExperienceLoaded will restart this player.
	if (IsExperienceLoaded())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
}

bool AArcMassGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	return Super::PlayerCanRestart_Implementation(Player);
}

void AArcMassGameMode::FailedToRestartPlayer(AController* NewPlayer)
{
	Super::FailedToRestartPlayer(NewPlayer);

	if (UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer))
	{
		if (APlayerController* NewPC = Cast<APlayerController>(NewPlayer))
		{
			if (PlayerCanRestart(NewPC))
			{
				RequestPlayerRestartNextFrame(NewPlayer, false);
			}
			else
			{
				UE_LOGF(LogArcMassGameMode
					, Verbose
					,"FailedToRestartPlayer(%ls) and PlayerCanRestart returned false; not retrying.", *GetPathNameSafe(NewPlayer));
			}
		}
		else
		{
			RequestPlayerRestartNextFrame(NewPlayer, false);
		}
	}
	else
	{
		UE_LOGF(LogArcMassGameMode
			, Verbose
			,"FailedToRestartPlayer(%ls) but no pawn class is resolvable; giving up.", *GetPathNameSafe(NewPlayer));
	}
}

void AArcMassGameMode::RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset)
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

void AArcMassGameMode::OnExperienceLoaded(const UArcMassExperienceDefinition* CurrentExperience)
{
	// Spawn any players that were already attached but pawnless when the experience landed.
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

bool AArcMassGameMode::IsExperienceLoaded() const
{
	if (GameState == nullptr)
	{
		return false;
	}

	const UArcMassExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcMassExperienceManagerComponent>();
	if (ExperienceComponent == nullptr)
	{
		return false;
	}

	return ExperienceComponent->IsExperienceLoaded();
}

void AArcMassGameMode::HandleMatchAssignmentIfNotExpectingOne()
{
	FString ExperienceIdSource;
	FPrimaryAssetId ExperienceId = GetExperiencePrimaryId(ExperienceIdSource);

	OnMatchAssignmentGiven(ExperienceId, ExperienceIdSource);
}

namespace ArcMassGameModeExperience
{
	FPrimaryAssetId FindExperienceIdByAssetName(const FString& AssetName)
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		TArray<FPrimaryAssetId> ExperienceIds;
		AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType(UArcMassExperienceDefinition::StaticClass()->GetFName()), ExperienceIds);

		for (const FPrimaryAssetId& ExperienceId : ExperienceIds)
		{
			if (AssetManager.GetPrimaryAssetPath(ExperienceId).GetAssetName() == AssetName)
			{
				return ExperienceId;
			}
		}

		return FPrimaryAssetId();
	}
}

FPrimaryAssetId AArcMassGameMode::GetExperiencePrimaryId(FString& OutExperienceIdSource) const
{
	FString ExperienceAssetName;

	if (UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
	{
		ExperienceAssetName = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
		OutExperienceIdSource = TEXT("OptionsString");
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), ExperienceAssetName))
	{
		OutExperienceIdSource = TEXT("CommandLine");
	}
	else
	{
		if (AArcMassWorldSettings* TypedWorldSettings = Cast<AArcMassWorldSettings>(GetWorldSettings()))
		{
			const FPrimaryAssetId ExperienceId = TypedWorldSettings->GetDefaultExperienceId();
			if (ExperienceId.IsValid())
			{
				OutExperienceIdSource = TEXT("WorldSettings");
			}
			return ExperienceId;
		}
		return FPrimaryAssetId();
	}

	const FPrimaryAssetId ExperienceId = ArcMassGameModeExperience::FindExperienceIdByAssetName(ExperienceAssetName);
	if (!ExperienceId.IsValid())
	{
		UE_LOGF(LogArcMassGameMode
			, Error
			,"No ArcMassExperienceDefinition asset named '%ls' (Source: %ls).", *ExperienceAssetName
			, *OutExperienceIdSource);
	}
	return ExperienceId;
}

void AArcMassGameMode::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId, const FString& ExperienceIdSource)
{
	if (ExperienceId.IsValid())
	{
		UE_LOGF(LogArcMassGameMode
			, Log
			,"Identified experience %ls (Source: %ls)", *ExperienceId.ToString()
			, *ExperienceIdSource);

		UArcMassExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UArcMassExperienceManagerComponent>();
		check(ExperienceComponent);
		ExperienceComponent->ServerSetCurrentExperience(ExperienceId);
	}
	else
	{
		const FString DefaultMap = UGameMapsSettings::GetGameDefaultMap();
		UWorld* World = GetWorld();
		UGameInstance* GameInstance = GetGameInstance();

		if (GameInstance && World && World->GetNetMode() == NM_DedicatedServer && World->URL.Map == DefaultMap)
		{
			UE_LOGF(LogArcMassGameMode
				, Log
				,"Loading default map without an experience. Map %ls", *DefaultMap);
		}
		else
		{
			UE_LOGF(LogArcMassGameMode
				, Error
				,"Failed to identify experience; loading screen will stay up.");
		}
	}
}
