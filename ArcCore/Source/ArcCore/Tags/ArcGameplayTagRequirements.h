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

#include "GameplayTagContainer.h"

#include "ArcGameplayTagRequirements.generated.h"

/**
 * Drop-in replacement for FGameplayTagRequirements (GameplayAbilities plugin).
 * Encapsulates require-tags, ignore-tags, and an optional tag query.
 * Allows modules to express tag gate logic without depending on GameplayAbilities.
 */
USTRUCT(BlueprintType)
struct FArcGameplayTagRequirements
{
	GENERATED_USTRUCT_BODY()

	/** All of these tags must be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Tags, meta = (DisplayName = "Must Have Tags"))
	FGameplayTagContainer RequireTags;

	/** None of these tags may be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Tags, meta = (DisplayName = "Must Not Have Tags"))
	FGameplayTagContainer IgnoreTags;

	/** Build up a more complex query that can't be expressed with RequireTags/IgnoreTags alone */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = Tags, meta = (DisplayName = "Query Must Match"))
	FGameplayTagQuery TagQuery;

	/** True if all required tags are present and no ignore tags are present */
	ARCCORE_API bool RequirementsMet(const FGameplayTagContainer& Container) const;

	/** True if RequireTags, IgnoreTags, and TagQuery are all empty */
	ARCCORE_API bool IsEmpty() const;

	/** Returns a debug string describing these requirements */
	ARCCORE_API FString ToString() const;

	/** Converts RequireTags and IgnoreTags fields into an equivalent FGameplayTagQuery */
	[[nodiscard]] ARCCORE_API FGameplayTagQuery ConvertTagFieldsToTagQuery() const;

	ARCCORE_API bool operator==(const FArcGameplayTagRequirements& Other) const;
	ARCCORE_API bool operator!=(const FArcGameplayTagRequirements& Other) const;
};
