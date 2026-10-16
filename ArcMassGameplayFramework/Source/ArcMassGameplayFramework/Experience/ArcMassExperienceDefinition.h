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

#include "Engine/DataAsset.h"
#include "Engine/AssetManager.h"

#include "ArcMassExperienceDefinition.generated.h"

class UGameFeatureAction;
class UArcMassPawnData;
class UArcMassExperienceActionSet;

/**
 * UArcMassExperienceDefinition
 *
 * Mass-native experience definition. Mirrors UArcExperienceDefinition without any
 * GAS-specific surface (no UArcExperienceData reference, no ability sets).
 */
UCLASS(BlueprintType, Const)
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UArcMassExperienceDefinition();

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
	/** List of actions to perform as this experience is loaded/activated/deactivated/unloaded. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Actions", meta = (DisplayThumbnail = false))
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** Reusable action sets layered on top of Actions. */
	UPROPERTY(EditDefaultsOnly, Category = "Actions", meta = (DisplayThumbnail = false))
	TArray<TObjectPtr<UArcMassExperienceActionSet>> ActionSets;

	/** List of GameFeature plugins this experience wants to have active. */
	UPROPERTY(EditAnywhere, Category = "Gameplay")
	TArray<FString> GameFeaturesToEnable;

	/** The default pawn data for players when their PlayerState does not provide one. */
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay", meta = (DisplayThumbnail = false))
	TSoftObjectPtr<UArcMassPawnData> DefaultPawnData;

public:
	TSoftObjectPtr<UArcMassPawnData> GetDefaultPawnData() const
	{
		return DefaultPawnData;
	}

	const TArray<UGameFeatureAction*>& GetActions() const
	{
		return Actions;
	}
};
