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
#include "GameplayTagContainer.h"
#include "ArcItemFactoryGrantedPassiveAbilities.generated.h"

USTRUCT(BlueprintType)
struct FArcItemFactoryGrantedPassiveAbilitySet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	FGameplayTag Pool;

	UPROPERTY(EditAnywhere, Category = "Data")
	float Weight;

	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<TSubclassOf<class UGameplayAbility>> GrantedPassiveAbilities;

	FArcItemFactoryGrantedPassiveAbilitySet()
		: Weight(1)
	{
	}
};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcItemFactoryGrantedPassiveAbilities : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcItemFactoryGrantedPassiveAbilitySet> GrantedPassiveAbilities;
};
