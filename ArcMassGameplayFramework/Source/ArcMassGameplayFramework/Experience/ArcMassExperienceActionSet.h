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

#include "ArcMassExperienceActionSet.generated.h"

class UGameFeatureAction;

/**
 * UArcMassExperienceActionSet
 *
 * Reusable bundle of GameFeatureActions and GameFeature plugin dependencies that
 * can be referenced from one or more UArcMassExperienceDefinition assets.
 */
UCLASS(BlueprintType, NotBlueprintable)
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassExperienceActionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UArcMassExperienceActionSet();

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
	/** List of actions to perform as this experience is loaded/activated/deactivated/unloaded. */
	UPROPERTY(EditAnywhere, Instanced, Category = "Actions to Perform")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	/** List of GameFeature plugins this experience wants to have active. */
	UPROPERTY(EditAnywhere, Category = "Feature Dependencies")
	TArray<FString> GameFeaturesToEnable;
};
