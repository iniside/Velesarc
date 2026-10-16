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
#include "StructUtils/InstancedStruct.h"
#include "ArcNamedPrimaryAssetId.h"
#include "ArcCraft/Recipe/ArcCraftContext.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"

#include "ArcRecipeModifierSource.generated.h"

struct FArcItemSpec;

/**
 * Base struct for recipe modifier source selectors.
 * Each source defines WHERE modifiers come from (a pool asset, a material property table,
 * or a static set). The base carries shared eligibility and selection fields.
 * Sources never contain inline modifier data; they always reference a leaf record.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Base struct for recipe modifier source selectors; defines where modifiers come from and carries shared eligibility and selection fields."))
struct ARCCRAFT_API FArcRecipeModifierSource
{
	GENERATED_BODY()

	/** Manual weight for selection priority among competing modifiers.
	 *  Higher weight = more desirable during slot resolution. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection", meta = (ClampMin = "0.001"))
	float Weight = 1.0f;

	/** Minimum average ingredient rarity step (R) required to activate this source.
	 *  0.0 = always eligible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eligibility", meta = (ClampMin = "0.0"))
	float MinAverageR = 0.0f;

	/**
	 * Evaluate this source and produce pending modifiers for slot-based resolution.
	 *
	 * @param ConsumedIngredients     The actual items matched to each ingredient slot.
	 * @param Context                 Slot rarity steps, average R, crafter skill margin and the decided output rarity.
	 * @return Pending modifiers to be resolved through the recipe's slot configuration.
	 */
	virtual TArray<FArcCraftPendingModifier> Evaluate(
		const TArray<FArcItemSpec>& ConsumedIngredients,
		const FArcCraftContext& Context) const;

	virtual ~FArcRecipeModifierSource() = default;
};

/**
 * Modifier source that selects entries from a random pool record.
 * The selection strategy is provided via an instanced FArcRandomPoolSelectionMode,
 * allowing new strategies without modifying this struct.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Random Pool", Tooltip = "Modifier source that selects entries from a random pool record using a configurable instanced selection strategy."))
struct ARCCRAFT_API FArcRecipeModifierSource_RandomPool : public FArcRecipeModifierSource
{
	GENERATED_BODY()

	/** The random pool record containing all possible entries. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source",
		meta = (AllowedTypes = "ArcRandomPool"))
	FArcNamedPrimaryAssetId PoolDefinition;

	/** Selection mode — determines how entries are picked from the pool.
	 *  Use "Simple Random" for weighted random rolls, or "Budget" for point-based selection. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Selection",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRandomPoolSelectionMode", ExcludeBaseStruct))
	FInstancedStruct SelectionMode;

	virtual TArray<FArcCraftPendingModifier> Evaluate(
		const TArray<FArcItemSpec>& ConsumedIngredients,
		const FArcCraftContext& Context) const override;
};

/**
 * Modifier source that evaluates a material property table record against consumed ingredients.
 * Supports tag-driven rule matching, R-gated modifier selection, and remedies
 * (extra ingredients, extra craft time).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Material Properties", Tooltip = "Modifier source that evaluates a material property table record against consumed ingredients with tag-driven rule matching and R-gated selection."))
struct ARCCRAFT_API FArcRecipeModifierSource_MaterialProperties : public FArcRecipeModifierSource
{
	GENERATED_BODY()

	/** The material property table record to evaluate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source",
		meta = (AllowedTypes = "ArcMaterialPropertyTable"))
	FArcNamedPrimaryAssetId PropertyTable;

	/** Base ingredient count for extra-materials remedy calculation.
	 *  Ingredients beyond this count add weight bonus. 0 = no extra-material bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remedies", meta = (ClampMin = "0"))
	int32 BaseIngredientCount = 0;

	/** Designer-configured weight bonus for extra time spent crafting.
	 *  Capped by the table's ExtraTimeWeightBonusCap. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Remedies")
	float ExtraCraftTimeBonus = 0.0f;

	virtual TArray<FArcCraftPendingModifier> Evaluate(
		const TArray<FArcItemSpec>& ConsumedIngredients,
		const FArcCraftContext& Context) const override;
};

/**
 * Modifier source that applies every modifier in a static modifier set record unconditionally
 * (subject to the base eligibility gate — MinAverageR).
 * No selection or weighting among the set's entries — all are applied.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Static Modifier Set", Tooltip = "Modifier source that applies every modifier in a static modifier set record unconditionally, subject to the base MinAverageR eligibility gate."))
struct ARCCRAFT_API FArcRecipeModifierSource_Static : public FArcRecipeModifierSource
{
	GENERATED_BODY()

	/** The static modifier set record. All entries are applied when eligible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source",
		meta = (AllowedTypes = "ArcStaticModifierSet"))
	FArcNamedPrimaryAssetId ModifierSet;

	virtual TArray<FArcCraftPendingModifier> Evaluate(
		const TArray<FArcItemSpec>& ConsumedIngredients,
		const FArcCraftContext& Context) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Transfer Stat Ranges (From Table)", Tooltip = "Modifier source that reads a stat transfer table record and sums each part's share-weighted stat roll into one result per stat."))
struct ARCCRAFT_API FArcRecipeModifierSource_StatTransferTable : public FArcRecipeModifierSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Source", meta = (AllowedTypes = "ArcStatTransferTable", Tooltip = "The stat transfer table record mapping ingredient slots to output stats."))
	FArcNamedPrimaryAssetId Table;

	virtual TArray<FArcCraftPendingModifier> Evaluate(
		const TArray<FArcItemSpec>& ConsumedIngredients,
		const FArcCraftContext& Context) const override;
};

namespace ArcCraftProvenance
{
	/** The RequiredTags that IngredientTags hold. */
	ARCCRAFT_API FGameplayTagContainer FilterHeldTags(
		const FGameplayTagContainer& RequiredTags,
		const FGameplayTagContainer& IngredientTags);

	/** Consumed amount of the slots holding any RequiredTags over the total consumed amount; 0 without requirements. */
	ARCCRAFT_API float ComputeIngredientShare(
		const FGameplayTagContainer& RequiredTags,
		const TArray<FGameplayTagContainer>& PerSlotTags,
		const TArray<int32>& SlotAmounts);
}
