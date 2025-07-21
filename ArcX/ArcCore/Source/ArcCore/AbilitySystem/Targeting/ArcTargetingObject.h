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
#include "StructUtils/InstancedStruct.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcTargetingObject.generated.h"

class UTargetingPreset;
class UArcAbilityTargetingComponent;
struct FArcCustomTraceDataHandle;
struct FGameplayAbilityTargetDataHandle;
class AArcTargetingVisualizationActor;

UCLASS(BlueprintType)
class ARCCORE_API UArcTargetingObject : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UTargetingPreset> TargetingPreset;

	// Use On Server To verify client targeting.
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UTargetingPreset> TargetingVerification;

	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TSoftClassPtr<AArcTargetingVisualizationActor> VisualizationActor;
	
};

USTRUCT(BlueprintType, meta = (Category = "Gameplay Ability"))
struct ARCCORE_API FArcItemFragment_TargetingObjectPreset : public FArcItemFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game", DisplayThumbnail = "false"))
	TObjectPtr<UArcTargetingObject> TargetingObject;
};

USTRUCT(BlueprintType, meta = (Category = "Gameplay Ability"))
struct ARCCORE_API FArcItemFragment_TargetingPreset : public FArcItemFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game", DisplayThumbnail = "false"))
	TObjectPtr<UTargetingPreset> TargetingPreset;
};

USTRUCT(BlueprintType, meta = (Category = "Gameplay Ability"))
struct ARCCORE_API FArcItemFragment_GlobalTargeting : public FArcItemFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GlobalTargeting"))
	FGameplayTag TargetingTag;
};