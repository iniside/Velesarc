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

#pragma once

#include "CoreMinimal.h"
#include "ModularGameMode.h"
#include "GameFramework/OnlineReplStructs.h"
#include "ArcCoreGameMode.generated.h"

class UArcPawnData;
class UArcExperienceDefinition;
enum class ECommonSessionOnlineMode : uint8;

DECLARE_LOG_CATEGORY_EXTERN(LogArcGameModeBase
	, Log
	, Log);

/**
 * Post login event, triggered when a player joins the game as well as after non-seamless
 * ServerTravel
 *
 * This is called after the player has finished initialization
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGameModeCombinedPostLogin
	, AGameModeBase* /*GameMode*/
	, AController* /*NewPlayer*/
);

/**
 * AArcGameMode
 *
 *	The base game mode class used by this project.
 */

/**
 *
 */
UCLASS()
class ARCCORE_API AArcCoreGameMode : public AModularGameModeBase
{
	GENERATED_BODY()

public:
	AArcCoreGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Try to get GameState class from Current Experience Definition. If not found fallback to default set in asset. */
	virtual void PreInitializeComponents() override;
	
	const UArcPawnData* GetPawnDataForController(const AController* InController) const;

	//~AGameModeBase interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual void FinishRestartPlayer(AController* NewPlayer, const FRotator& StartRotation) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual void InitGameState() override;
	virtual bool UpdatePlayerStartSpot(AController* Player, const FString& Portal, FString& OutErrorMessage) override;
	virtual void GenericPlayerInitialization(AController* NewPlayer) override;
	virtual void FailedToRestartPlayer(AController* NewPlayer) override;
	virtual APlayerController* SpawnPlayerController(ENetRole InRemoteRole, const FString& Options) override;
	virtual void OnPostLogin(AController* NewPlayer) override;

	//~End of AGameModeBase interface

	FOnGameModeCombinedPostLogin& OnGameModeCombinedPostLogin()
	{
		return OnGameModeCombinedPostLoginDelegate;
	}

	// Restart (respawn) the specified player or bot next frame
	UFUNCTION(BlueprintCallable)
	void RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset = false);

private:
	FOnGameModeCombinedPostLogin OnGameModeCombinedPostLoginDelegate;

protected:
	void OnExperienceLoaded(const UArcExperienceDefinition* CurrentExperience);

	bool IsExperienceLoaded() const;

	void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId, const FString& ExperienceIdSource);

	void HandleMatchAssignmentIfNotExpectingOne();

	/** Try to find current experience PrimaryId and write out where it came from. */
	FPrimaryAssetId GetExperiencePrimaryId(FString& OutExperienceIdSource) const;

public:
	/**
	 * Try to get current experience definition asset, without assiging it to experience manager component.
	 * This allows us to use some of it properties during Game Mode initialization.
	 */
	UArcExperienceDefinition* GetCurrentExperienceDefinition() const;
};
