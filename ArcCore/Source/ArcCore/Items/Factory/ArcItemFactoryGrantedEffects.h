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
#include "GameplayTagContainer.h"
#include "ArcItemFactoryGrantedEffects.generated.h"

USTRUCT(BlueprintType)
struct FArcItemFactoryGrantedEffectSet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	FGameplayTag Pool;

	UPROPERTY(EditAnywhere, Category = "Data")
	float Weight;

	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<TSubclassOf<class UGameplayEffect>> Effects;

	FArcItemFactoryGrantedEffectSet()
		: Weight(1)
		, Effects()
	{
	}
};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcItemFactoryGrantedEffects : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcItemFactoryGrantedEffectSet> GrantedEffects;
};
