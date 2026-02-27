/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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
#include "GameplayTagContainer.h"
#include "Items/ArcItemTypes.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/SubclassOf.h"

#include "ArcCraftModifier.generated.h"

struct FArcItemSpec;
class UGameplayEffect;
class UArcRandomPoolDefinition;

/**
 * Base struct for terminal craft modifiers.
 * Used inside quality bands (FArcMaterialQualityBand) and random pool entries (FArcRandomPoolEntry).
 *
 * Unlike FArcRecipeOutputModifier (recipe-level), these are terminal — they cannot
 * recurse into Random, RandomPool, MaterialProperties, or TransferStats.
 * Only Stats, Abilities, and Effects subtypes exist.
 *
 * All subtypes share common triggering/selection fields:
 *  - MinQualityThreshold: quality gate (0 = always)
 *  - TriggerTags: ingredient tag requirement (empty = always match)
 *  - Weight: manual priority for selection among multiple matches
 *  - QualityScalingFactor: how quality affects output values
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Minimum quality threshold. Only trigger if effective quality >= this value. 0 = always. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	float MinQualityThreshold = 0.0f;

	/** Tags that must ALL be present on aggregated ingredient tags for this modifier to trigger.
	 *  Empty = always match (no tag requirement). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FGameplayTagContainer TriggerTags;

	/** Manual weight for selection priority among multiple matching modifiers.
	 *  Higher weight = more likely to be chosen when there are competing matches. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier", meta = (ClampMin = "0.001"))
	float Weight = 1.0f;

	/** How much quality affects output values.
	 *  0.0 = quality has no effect, 1.0 = linear scaling, 2.0 = double scaling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	float QualityScalingFactor = 1.0f;

	/** Check if this modifier's trigger tags match the given ingredient tags.
	 *  Returns true if TriggerTags is empty (always match) or if Tags.HasAll(TriggerTags). */
	bool MatchesTriggerTags(const FGameplayTagContainer& IngredientTags) const;

	/** Apply this modifier to the output item spec.
	 *  @param OutItemSpec               The output item spec being built.
	 *  @param AggregatedIngredientTags  Union of all ingredient tags.
	 *  @param EffectiveQuality          Quality value (may be modified by band/pool context). */
	virtual void Apply(
		FArcItemSpec& OutItemSpec,
		const FGameplayTagContainer& AggregatedIngredientTags,
		float EffectiveQuality) const;

	virtual ~FArcCraftModifier() = default;
};

/**
 * Adds a stat to the output item, scaled by quality.
 * Formula: FinalValue = BaseValue * (1.0 + (EffectiveQuality - 1.0) * QualityScalingFactor)
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Stat Modifier"))
struct ARCCRAFT_API FArcCraftModifier_Stats : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Base stat to add to the output item. */
	UPROPERTY(EditAnywhere, Category = "Stats")
	FArcItemAttributeStat BaseStat;

	virtual void Apply(
		FArcItemSpec& OutItemSpec,
		const FGameplayTagContainer& AggregatedIngredientTags,
		float EffectiveQuality) const override;
};

/**
 * Grants an ability on the output item when trigger conditions are met.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Ability Modifier"))
struct ARCCRAFT_API FArcCraftModifier_Abilities : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Ability to grant when trigger conditions are met. */
	UPROPERTY(EditAnywhere, Category = "Abilities")
	FArcAbilityEntry AbilityToGrant;

	virtual void Apply(
		FArcItemSpec& OutItemSpec,
		const FGameplayTagContainer& AggregatedIngredientTags,
		float EffectiveQuality) const override;
};

/**
 * Grants a gameplay effect on the output item when trigger conditions and quality threshold are met.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Effect Modifier"))
struct ARCCRAFT_API FArcCraftModifier_Effects : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Gameplay effect to grant when trigger conditions are met. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	TSubclassOf<UGameplayEffect> EffectToGrant;

	virtual void Apply(
		FArcItemSpec& OutItemSpec,
		const FGameplayTagContainer& AggregatedIngredientTags,
		float EffectiveQuality) const override;
};

/**
 * Delegates to a UArcRandomPoolDefinition for entry selection.
 * Runs the pool's eligibility, weighting, and selection pipeline using the band's
 * effective quality and aggregated ingredient tags as context.
 *
 * The pool itself contains only terminal modifiers (Stats/Abilities/Effects),
 * so there is no recursion risk.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Random Pool Modifier"))
struct ARCCRAFT_API FArcCraftModifier_RandomPool : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** The pool definition asset containing all possible entries. */
	UPROPERTY(EditAnywhere, Category = "RandomPool")
	TSoftObjectPtr<UArcRandomPoolDefinition> PoolDefinition;

	/** Selection mode — determines how entries are picked from the pool.
	 *  Use "Simple Random" for weighted random rolls, or "Budget" for point-based selection. */
	UPROPERTY(EditAnywhere, Category = "RandomPool",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRandomPoolSelectionMode", ExcludeBaseStruct))
	FInstancedStruct SelectionMode;

	virtual void Apply(
		FArcItemSpec& OutItemSpec,
		const FGameplayTagContainer& AggregatedIngredientTags,
		float EffectiveQuality) const override;
};
