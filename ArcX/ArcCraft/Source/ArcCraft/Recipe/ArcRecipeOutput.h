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
#include "GameplayTagContainer.h"
#include "Items/ArcItemTypes.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/SubclassOf.h"

#include "ArcRecipeOutput.generated.h"

struct FArcItemSpec;
struct FArcItemData;
class UGameplayEffect;
class UChooserTable;
class UArcRandomModifierEntry;
class UArcRandomPoolDefinition;

/**
 * Base struct for output modifier rules.
 * Each modifier defines how ingredients affect the crafted output item.
 * Uses instanced struct pattern for extensibility.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRecipeOutputModifier
{
	GENERATED_BODY()

public:
	/**
	 * Evaluate and apply this modifier to the output item spec.
	 *
	 * @param OutItemSpec             The output item spec being built.
	 * @param ConsumedIngredients     The actual items matched to each ingredient slot.
	 * @param IngredientQualityMults  Quality multiplier for each ingredient slot (parallel array).
	 * @param AverageQuality          Weighted average quality across all ingredients.
	 */
	virtual void ApplyToOutput(
		FArcItemSpec& OutItemSpec,
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float AverageQuality) const;

	virtual ~FArcRecipeOutputModifier() = default;
};

/**
 * Adds stats to the output item, scaled by ingredient quality.
 * Formula: FinalValue = BaseValue * (1.0 + (AverageQuality - 1.0) * QualityScalingFactor)
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Stat Modifier"))
struct ARCCRAFT_API FArcRecipeOutputModifier_Stats : public FArcRecipeOutputModifier
{
	GENERATED_BODY()

public:
	/** Base stats to add to the output item. */
	UPROPERTY(EditAnywhere, Category = "Stats")
	TArray<FArcItemAttributeStat> BaseStats;

	/** How much quality affects stats.
	 *  0.0 = quality has no effect, 1.0 = linear scaling, 2.0 = double scaling. */
	UPROPERTY(EditAnywhere, Category = "Stats")
	float QualityScalingFactor = 1.0f;

	virtual void ApplyToOutput(
		FArcItemSpec& OutItemSpec,
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float AverageQuality) const override;
};

/**
 * Grants abilities on the output item if any consumed ingredient has ALL of the trigger tags.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Ability Modifier"))
struct ARCCRAFT_API FArcRecipeOutputModifier_Abilities : public FArcRecipeOutputModifier
{
	GENERATED_BODY()

public:
	/** Tags that must ALL be present on any single consumed ingredient to trigger this modifier. */
	UPROPERTY(EditAnywhere, Category = "Abilities")
	FGameplayTagContainer TriggerTags;

	/** Abilities to grant when trigger tags are present. */
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<FArcAbilityEntry> AbilitiesToGrant;

	virtual void ApplyToOutput(
		FArcItemSpec& OutItemSpec,
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float AverageQuality) const override;
};

/**
 * Grants gameplay effects on the output item based on ingredient tags and quality threshold.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Effect Modifier"))
struct ARCCRAFT_API FArcRecipeOutputModifier_Effects : public FArcRecipeOutputModifier
{
	GENERATED_BODY()

public:
	/** Tags that must ALL be present on any single consumed ingredient to trigger this modifier. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	FGameplayTagContainer TriggerTags;

	/** Gameplay effects to grant. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	TArray<TSubclassOf<UGameplayEffect>> EffectsToGrant;

	/** Only grant if average quality multiplier exceeds this threshold. 0 = always grant. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	float MinQualityThreshold = 0.0f;

	virtual void ApplyToOutput(
		FArcItemSpec& OutItemSpec,
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float AverageQuality) const override;
};

/**
 * Transfers stats from a specific ingredient slot to the output item with optional scaling.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Ingredient Stat Transfer"))
struct ARCCRAFT_API FArcRecipeOutputModifier_TransferStats : public FArcRecipeOutputModifier
{
	GENERATED_BODY()

public:
	/** Index of the ingredient slot to transfer stats from. */
	UPROPERTY(EditAnywhere, Category = "Transfer")
	int32 IngredientSlotIndex = 0;

	/** Scale factor applied to transferred stats. */
	UPROPERTY(EditAnywhere, Category = "Transfer")
	float TransferScale = 1.0f;

	/** If true, the ingredient's quality multiplier also scales the transferred stats. */
	UPROPERTY(EditAnywhere, Category = "Transfer")
	bool bScaleByQuality = true;

	virtual void ApplyToOutput(
		FArcItemSpec& OutItemSpec,
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float AverageQuality) const override;
};

/**
 * Evaluates a UChooserTable at craft time to randomly select modifier entries.
 * The chooser table filters and weights rows based on ingredient tags, quality,
 * and other context from FArcRecipeCraftContext.
 *
 * Each selected row returns a UArcRandomModifierEntry (as the chooser's UObject result),
 * whose modifiers are then applied to the output item.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Random Modifier (Chooser)"))
struct ARCCRAFT_API FArcRecipeOutputModifier_Random : public FArcRecipeOutputModifier
{
	GENERATED_BODY()

public:
	/** The ChooserTable asset to evaluate. Rows should return UArcRandomModifierEntry objects. */
	UPROPERTY(EditAnywhere, Category = "Random")
	TSoftObjectPtr<UChooserTable> ModifierChooserTable;

	/** Number of times to roll (evaluate) the chooser table. Each roll can produce one entry. */
	UPROPERTY(EditAnywhere, Category = "Random", meta = (ClampMin = 1))
	int32 MaxRolls = 1;

	/** If false, the same UArcRandomModifierEntry cannot be selected more than once. */
	UPROPERTY(EditAnywhere, Category = "Random")
	bool bAllowDuplicates = false;

	/** If true, quality above QualityBonusRollThreshold grants additional rolls. */
	UPROPERTY(EditAnywhere, Category = "Random|Quality")
	bool bQualityAffectsRolls = false;

	/** Average quality multiplier threshold at which a bonus roll is granted.
	 *  Each full multiple of this threshold adds one bonus roll. */
	UPROPERTY(EditAnywhere, Category = "Random|Quality",
		meta = (EditCondition = "bQualityAffectsRolls", ClampMin = "0.01"))
	float QualityBonusRollThreshold = 2.0f;

	virtual void ApplyToOutput(
		FArcItemSpec& OutItemSpec,
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float AverageQuality) const override;
};

/**
 * Self-contained random modifier that selects entries from a UArcRandomPoolDefinition.
 * Selection strategy is determined by an instanced struct (FArcRandomPoolSelectionMode),
 * allowing new strategies to be added without modifying this struct.
 *
 * Unlike FArcRecipeOutputModifier_Random (Chooser-based), this requires no external
 * ChooserTable or per-row data assets — all entries are defined inline in the pool asset.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Random Modifier (Pool)"))
struct ARCCRAFT_API FArcRecipeOutputModifier_RandomPool : public FArcRecipeOutputModifier
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

	virtual void ApplyToOutput(
		FArcItemSpec& OutItemSpec,
		const TArray<const FArcItemData*>& ConsumedIngredients,
		const TArray<float>& IngredientQualityMults,
		float AverageQuality) const override;
};
