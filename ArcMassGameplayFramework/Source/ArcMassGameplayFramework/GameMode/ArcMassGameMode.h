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

#pragma once

#include "Engine/AssetManager.h"
#include "ModularGameMode.h"

#include "ArcMassGameMode.generated.h"

class AController;
class APawn;
class APlayerController;
class UArcMassPawnData;
class UArcMassExperienceDefinition;

DECLARE_LOG_CATEGORY_EXTERN(LogArcMassGameMode, Log, Log);

/**
 * AArcMassGameMode
 *
 * Modular GameMode for the Mass-driven game framework. Mirrors AArcCoreGameMode's
 * pawn-spawning surface but is intentionally free of GAS plumbing. Experience-system
 * integration is stubbed and will be wired up in step 9.
 */
UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API AArcMassGameMode : public AModularGameModeBase
{
	GENERATED_BODY()

public:
	AArcMassGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Resolves the pawn data for the given controller. Returns nullptr until step 9 wires the experience system. */
	const UArcMassPawnData* GetPawnDataForController(const AController* InController) const;

	//~AGameModeBase interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
	virtual void FailedToRestartPlayer(AController* NewPlayer) override;
	//~End of AGameModeBase interface

	/** Restart (respawn) the specified player or bot next frame. */
	UFUNCTION(BlueprintCallable)
	void RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset = false);

protected:
	/** Hook invoked once the active experience finishes loading; restarts pawnless players. */
	void OnExperienceLoaded(const UArcMassExperienceDefinition* CurrentExperience);

	/** Returns true if the GameState's experience manager component reports loaded. */
	bool IsExperienceLoaded() const;

	/** Resolve and forward the chosen experience id to the experience manager component. */
	void HandleMatchAssignmentIfNotExpectingOne();

	/** Apply a resolved experience id to the experience manager component (server-side). */
	void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId, const FString& ExperienceIdSource);

public:
	/** Resolve the active experience id from URL options / command line / world / dev settings. */
	FPrimaryAssetId GetExperiencePrimaryId(FString& OutExperienceIdSource) const;
};
