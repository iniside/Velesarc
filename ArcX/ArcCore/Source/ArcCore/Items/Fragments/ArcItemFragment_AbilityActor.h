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

#include "Templates/SubclassOf.h"

#include "ArcItemFragment.h"
#include "ArcItemFragment_AbilityActor.generated.h"

class UArcActorGameplayAbility;
class AArcAbilityActor;

USTRUCT(BlueprintType)
struct FArcAbilityActorItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	TSoftClassPtr<AActor> AbilityActorClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	TSubclassOf<UArcActorGameplayAbility> AbilityClass;
};

/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemFragment_AbilityActor : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game"))
	FArcAbilityActorItem AbilityActorClass;
};


/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemFragment_AbilityActorMap : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AssetBundles = "Game", ForceInlineRow))
	TMap<FGameplayTag, FArcAbilityActorItem> AbilityActorClasses;
};