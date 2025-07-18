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
#include "ScalableFloat.h"

#include "ArcCore/Items/ArcItemTypes.h"

#include "ArcItemFactoryAttributes.generated.h"

class UGameplayEffect;

USTRUCT(BlueprintType)
struct FArcFactoryItemAttribute
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	FGameplayAttribute Attribute;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (Categories = "SetByCaller"))
	FGameplayTag SetByCallerTag;

	UPROPERTY(EditAnywhere, Category = "Data")
	FScalableFloat Min;

	UPROPERTY(EditAnywhere, Category = "Data")
	FScalableFloat Max;

	FArcFactoryItemAttribute()
		: Attribute()
		, SetByCallerTag(FGameplayTag::EmptyTag)
		, Min(0)
		, Max(0)
	{
	}
};

USTRUCT(BlueprintType)
struct FArcFactoryItemAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	FGameplayTag PoolName;

	UPROPERTY(EditAnywhere, Category = "Data", meta = (TitleProperty = "SetByCallerTag"))
	TArray<FArcFactoryItemAttribute> AttributePool;

	UPROPERTY(EditAnywhere, Category = "Data")
	TSubclassOf<UGameplayEffect> GenGiveStatsEffect;

	FArcFactoryItemAttributeSet()
		: PoolName(FGameplayTag::EmptyTag)
		, AttributePool()
		, GenGiveStatsEffect(nullptr)
	{
	}
};

/**
 *
 */
UCLASS()
class ARCCORE_API UArcItemFactoryAttributes : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Data")
	TArray<FArcFactoryItemAttributeSet> Attributes;
};
