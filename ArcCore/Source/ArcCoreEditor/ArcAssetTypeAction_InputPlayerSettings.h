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
#include "ArcCoreEditorModule.h"


#include "AssetTypeActions/AssetTypeActions_Blueprint.h"

#include "Factories/BlueprintFactory.h"
#include "Factories/Factory.h"

#include "AssetDefinitionDefault.h"

#include "ArcAssetTypeAction_InputPlayerSettings.generated.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_ArcCore"

// FArcAssetTypeAction_InputActionConfig / UArcAssetDefinition_InputActionConfig /
// UArcInputActionConfig_Factory retired with the ArcCore Input cluster (Bucket B Cluster 1).

// UArcAssetDefinition_PawnData / UArcPawnData_Factory retired with UArcPawnData
// (Phase B - replaced by UArcMassPawnData in ArcMassGameplayFramework).

// UArcAssetDefinition_ExperienceActionSet / UArcExperienceActionSet_Factory /
// UArcAssetDefinition_ExperienceDefinition / UArcExperienceDefinition_Factory /
// UArcAssetDefinition_ExperienceData / UArcExperienceData_Factory retired with the
// Experience cluster (Bucket B Cluster 2).

// UArcAssetDefinition_AbilitySet / UArcAbilitySet_Factory retired with GAS keystone (Bucket D 7).

UCLASS()
class UArcComponentFactory : public UBlueprintFactory
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TSubclassOf<UBlueprint> BlueprintClassType;

	UPROPERTY(EditAnywhere
		, Category = BlueprintFactory
		, meta = (AllowAbstract = "", BlueprintBaseOnly = ""))
	TSubclassOf<class UObject> BaseClass;

	UArcComponentFactory();

	virtual bool CanCreateNew() const override
	{
		return false;
	};

	virtual bool ConfigureProperties() override;

	virtual UObject* FactoryCreateNew(UClass* Class
									  , UObject* InParent
									  , FName Name
									  , EObjectFlags Flags
									  , UObject* Context
									  , FFeedbackContext* Warn
									  , FName CallingContext) override;
};

// UArcItemsStoreComponent_Factory + FArcAssetTypeActions_ItemsStoreComponent retired
// with UArcItemsStoreComponent (plan step 8c).

#undef LOCTEXT_NAMESPACE
