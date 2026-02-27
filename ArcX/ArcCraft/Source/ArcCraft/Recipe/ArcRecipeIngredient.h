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

#include "CoreMinimal.h"
#include "ArcNamedPrimaryAssetId.h"
#include "GameplayTagContainer.h"

#include "ArcRecipeIngredient.generated.h"

struct FArcItemSpec;
class UArcQualityTierTable;

/**
 * Base ingredient requirement for a recipe slot.
 * Uses instanced struct pattern for extensibility — new ingredient types
 * can be added without modifying the recipe definition.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRecipeIngredient
{
	GENERATED_BODY()

public:
	/** Display name for the ingredient slot (for UI). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText SlotName;

	/** How many of this ingredient are needed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Amount = 1;

	/** Whether this ingredient is consumed during crafting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bConsumeOnCraft = true;

	/** Returns true if the given item satisfies this ingredient requirement. */
	virtual bool DoesItemSatisfy(
		const FArcItemSpec& InItemSpec,
		const UArcQualityTierTable* InTierTable) const;

	/** Get the quality multiplier that this specific item provides for this slot. */
	virtual float GetItemQualityMultiplier(
		const FArcItemSpec& InItemSpec,
		const UArcQualityTierTable* InTierTable) const;

	virtual ~FArcRecipeIngredient() = default;
};

/**
 * Ingredient matched by a specific item definition.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Specific Item Ingredient"))
struct ARCCRAFT_API FArcRecipeIngredient_ItemDef : public FArcRecipeIngredient
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly,
		meta = (AllowedClasses = "/Script/ArcCore.ArcItemDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId ItemDefinitionId;

	virtual bool DoesItemSatisfy(
		const FArcItemSpec& InItemSpec,
		const UArcQualityTierTable* InTierTable) const override;

	virtual float GetItemQualityMultiplier(
		const FArcItemSpec& InItemSpec,
		const UArcQualityTierTable* InTierTable) const override;
};

/**
 * Ingredient matched by gameplay tags with optional quality tier requirement.
 * The most flexible ingredient type — matches any item with the required tags
 * that doesn't have the deny tags, and optionally meets a minimum quality tier.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Tag-Based Ingredient"))
struct ARCCRAFT_API FArcRecipeIngredient_Tags : public FArcRecipeIngredient
{
	GENERATED_BODY()

public:
	/** Tags the item MUST have. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer RequiredTags;

	/** Tags the item must NOT have. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer DenyTags;

	/** Minimum quality tier tag the item must match or exceed.
	 *  If invalid, no minimum tier is required. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag MinimumTierTag;

	virtual bool DoesItemSatisfy(
		const FArcItemSpec& InItemSpec,
		const UArcQualityTierTable* InTierTable) const override;

	virtual float GetItemQualityMultiplier(
		const FArcItemSpec& InItemSpec,
		const UArcQualityTierTable* InTierTable) const override;
};
