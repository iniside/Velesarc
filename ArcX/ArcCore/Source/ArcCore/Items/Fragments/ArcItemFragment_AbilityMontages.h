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

#include "ArcItemFragment.h"
#include "GameplayTagContainer.h"

#include "ArcItemFragment_AbilityMontages.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct ARCCORE_API FArcEventMontageItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages", meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	FName StartSection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	float DesiredPlayTime = 1.0f;
};

/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcItemFragment_AbilityMontages : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages", meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UAnimMontage> StartMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	FName StartSection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages")
	float DesiredPlayTime = 1.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Montages", meta = (ForceInlineRow, AssetBundles = "Game"))
	TMap<FGameplayTag, FArcEventMontageItem> EventTagToMontageMap;
};
