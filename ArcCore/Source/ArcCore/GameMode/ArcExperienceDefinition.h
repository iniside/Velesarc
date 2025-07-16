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
#include "Engine/DataAsset.h"
#include "StructUtils/InstancedStruct.h"
#include "Core/ArcExperienceBundleState.h"
#include "ArcExperienceDefinition.generated.h"

class AArcCoreGameState;
class AArcCorePlayerState;
class UGameFeatureAction;
class UArcPawnData;
class UArcExperienceData;

class UGameFeatureAction;

/**
 * Definition of a set of actions to perform as part of entering an experience
 */
UCLASS(BlueprintType, NotBlueprintable)
class ARCCORE_API UArcExperienceActionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UArcExperienceActionSet();

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End of UObject interface

	//~UPrimaryDataAsset interface
#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
	//~End of UPrimaryDataAsset interface

public:
	// List of actions to perform as this experience is
	// loaded/activated/deactivated/unloaded
	UPROPERTY(EditAnywhere, Instanced, Category = "Actions to Perform")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	// List of Game Feature Plugins this experience wants to have active
	UPROPERTY(EditAnywhere, Category = "Feature Dependencies")
	TArray<FString> GameFeaturesToEnable;
};

UCLASS(BlueprintType, Const)
class ARCCORE_API UArcPlayerControllerData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	friend class UArcExperienceDefinition;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Game Mode", meta = (BaseStruct = "/Script/ArcCore.ArcPlayerControllerComponent"))
	TArray<FInstancedStruct> Components;
	
public:
	virtual void GiveComponents(class AArcCorePlayerController* InPlayerController) const;

};

/**
 * Definition of an experience
 */
UCLASS(BlueprintType, Const)
class ARCCORE_API UArcExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UArcExperienceDefinition();

	//~UObject interface
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	//~End of UObject interface

	//~UPrimaryDataAsset interface
#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif
	//~End of UPrimaryDataAsset interface
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
protected:
	UPROPERTY(VisibleAnywhere)
	FGuid ExperienceId;
	
public:
	/** List of actions to perform as this experience is
	 * loaded/activated/deactivated/unloaded */
	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Actions", meta = (DisplayThumbnail = false))
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	UPROPERTY(EditDefaultsOnly, Category = Actions, meta = (DisplayThumbnail = false))
	TArray<TObjectPtr<UArcExperienceActionSet>> ActionSets;

	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	FArcExperienceBundleState BundlesState;
	
	/** List of Game Feature Plugins this experience wants to have active */
	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TArray<FString> GameFeaturesToEnable;

	/**
	 * Player State and Player Controller which should override values from GameMode with this experience.
	 *
	 * These classes are first checked inside *Data assets, then here, and finally defaults from game mode
	 * are used if nothing found.
	 */
	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TSoftClassPtr<AArcCorePlayerState> PlayerStateClass;

	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TSoftClassPtr<AArcCorePlayerController> PlayerControllerClass;

	UPROPERTY(EditDefaultsOnly, Category = Gameplay)
	TSoftClassPtr<AArcCoreGameState> GameStateClass;
	
	/** The default pawn class to spawn for players */
	//@TODO: Make soft?
	UPROPERTY(EditDefaultsOnly, Category = Gameplay, meta = (DisplayThumbnail = false))
	TSoftObjectPtr<UArcPawnData> DefaultPawnData;

	UPROPERTY(EditDefaultsOnly, Category = Gameplay, meta = (DisplayThumbnail = false))
	TSoftObjectPtr<UArcExperienceData> DefaultGameData;

public:
	TSoftObjectPtr<UArcPawnData> GetDefaultPawnData() const
	{
		return DefaultPawnData;
	}

	TSoftObjectPtr<UArcExperienceData> GetDefaultGameData() const
	{
		return DefaultGameData;
	}

	const TArray<UGameFeatureAction*>& GetActions() const
	{
		return Actions;
	}
};
