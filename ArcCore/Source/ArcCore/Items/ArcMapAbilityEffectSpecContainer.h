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

#include "ArcItemId.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"

#include "ArcMapAbilityEffectSpecContainer.generated.h"

struct FArcItemFragment_AbilityEffectsToApply;

/**
 * Contains created effect specs, with source object and set ability system.
 */
USTRUCT(BlueprintType)
struct ARCCORE_API FArcEffectSpecItem
{
	GENERATED_BODY()

public:
	FGameplayTag SpecTag;

	// Item which have this spec;
	FArcItemId ItemId;

	// make copy or make pointers ?
	// All of those tags must be present.
	FGameplayTagContainer SourceRequiredTags;

	// None of these tags must be present;
	FGameplayTagContainer SourceIgnoreTags;

	// All of those tags must be present.
	FGameplayTagContainer TargetRequiredTags;

	// None of these tags must be present;
	FGameplayTagContainer TargetIgnoreTags;

	TArray<FGameplayEffectSpecHandle> Specs;

	bool operator==(const FArcEffectSpecItem& InKey) const
	{
		return ItemId == InKey.ItemId;
	}

	bool operator==(const FArcItemId& InKey) const
	{
		return ItemId == InKey;
	}
};

/*
	This container should be used, when you define it outside of ability, and there are
   several abilities that might need access to their own effects. when At design time you
   don't know how many abilities there will be.
 */

struct ARCCORE_API FArcMapAbilityEffectSpecContainer
{
	
public:
	TArray<FArcEffectSpecItem> SpecsArray;

	static void AddAbilityEffectSpecs(const FArcItemFragment_AbilityEffectsToApply* InContainer
											  , const FArcItemId& ItemHandle
											  , class UArcItemsStoreComponent* ItemsComponent
											  , FArcMapAbilityEffectSpecContainer& InOutContainer);

	static void RemoveAbilityEffectSpecs(FArcItemId ItemHandle
													 , FArcMapAbilityEffectSpecContainer& InOutContainer);
};

