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
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/SubclassOf.h"
#include "ItemFragments/ArcItemFragment_GrantedMassAbilities.h"
#include "Modifiers/ArcDirectModifier.h"
#include "ArcNamedPrimaryAssetId.h"

#include "ArcCraftModifier.generated.h"

struct FArcCraftContext;
struct FArcCraftRulesData;
struct FArcStatCatalogEntry;
struct FArcCraftPendingModifier;
class UArcEffectDefinition;

/**
 * Base struct for terminal craft modifiers.
 * Used inside material property rules (FArcMaterialPropertyRule), random pool entries (FArcRandomPoolEntry)
 * and static modifier sets.
 *
 * All subtypes share common triggering/selection fields:
 *  - MinR: R gate (0 = always)
 *  - TriggerTags: ingredient tag requirement (empty = always match)
 *  - SlotTags: chair tags the modifier's results compete for (empty = unslotted)
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Base struct for terminal craft modifiers used inside material property rules, random pool entries and static modifier sets."))
struct ARCCRAFT_API FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Minimum R. Only trigger if effective R >= this value. 0 = always. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	float MinR = 0.0f;

	/** Tags that must ALL be present on aggregated ingredient tags for this modifier to trigger.
	 *  Empty = always match (no tag requirement). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier")
	FGameplayTagContainer TriggerTags;

	/** Chair tags this modifier's results carry (Modifier.Stat, Modifier.Passive, Modifier.Penalty).
	 *  Empty = unslotted (always applies, bypasses slot resolution). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier", meta = (Categories = "Modifier"))
	FGameplayTagContainer SlotTags;

	/** Check if this modifier's trigger tags match the given ingredient tags.
	 *  Returns true if TriggerTags is empty (always match) or if Tags.HasAll(TriggerTags). */
	bool MatchesTriggerTags(const FGameplayTagContainer& IngredientTags) const;

	virtual ~FArcCraftModifier() = default;
};

/**
 * Adds a catalog stat to the output: a wearer modifier, or an item stat when the catalog entry is bItemSide.
 * The value rolls in the output rarity's band of the catalog range (ArcCraftRarity::ComputeBandValue).
 * With Modifier.Penalty in SlotTags it applies -value*Sign on the wearer, or Multiply 1 - value on the item.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Stat Modifier", Tooltip = "Adds a stat from the craft rules' stat catalog; the value rolls in the output rarity's band."))
struct ARCCRAFT_API FArcCraftModifier_Stats : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Stat id in the stat catalog named by the craft rules (FArcCraftRulesData::StatCatalog). */
	UPROPERTY(EditAnywhere, Category = "Stats", meta = (Tooltip = "Stat id in the craft rules' stat catalog."))
	FName StatId;
};

/**
 * Grants an ability on the output item when trigger conditions are met.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Ability Modifier", Tooltip = "Grants an ability on the output item when trigger conditions are met."))
struct ARCCRAFT_API FArcCraftModifier_Abilities : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Ability entry (definition + optional input tag) to grant when trigger conditions are met. */
	UPROPERTY(EditAnywhere, Category = "Abilities", meta = (Tooltip = "Ability entry granted when trigger conditions are met."))
	FArcMassAbilityEntry AbilityEntry;
};

/**
 * Grants a gameplay effect on the output item when trigger conditions and R threshold are met.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Effect Modifier", Tooltip = "Grants a gameplay effect on the output item when trigger conditions and R threshold are met."))
struct ARCCRAFT_API FArcCraftModifier_Effects : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Mass effect definition to grant when trigger conditions are met. */
	UPROPERTY(EditAnywhere, Category = "Effects", meta = (Tooltip = "Effect granted when trigger conditions are met."))
	TObjectPtr<UArcEffectDefinition> Effect;
};

/**
 * Adds one fixed direct attribute modifier to the output item.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Attribute Modifier", Tooltip = "Adds one fixed direct attribute modifier to the output item."))
struct ARCCRAFT_API FArcCraftModifier_AttributeModifier : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** Single direct attribute modifier applied to the Mass entity. No backing GE required. */
	UPROPERTY(EditAnywhere, Category = "AttributeModifier", meta = (Tooltip = "Direct attribute modifier applied to the entity; no backing GE required."))
	FArcDirectModifier Modifier;
};

/**
 * Delegates to a random pool record for entry selection.
 * Runs the pool's eligibility, weighting, and selection pipeline using the craft's
 * average R and aggregated ingredient tags as context.
 *
 * The pool itself contains only terminal modifiers (Stats/Abilities/Effects),
 * so there is no recursion risk.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Craft Random Pool Modifier", Tooltip = "Delegates to a random pool record for entry selection using the craft's average R and aggregated ingredient tags as context."))
struct ARCCRAFT_API FArcCraftModifier_RandomPool : public FArcCraftModifier
{
	GENERATED_BODY()

public:
	/** The random pool record containing all possible entries. */
	UPROPERTY(EditAnywhere, Category = "RandomPool", meta = (AllowedTypes = "ArcRandomPool", Tooltip = "The random pool record containing all possible entries."))
	FArcNamedPrimaryAssetId PoolDefinition;

	/** Selection mode — determines how entries are picked from the pool.
	 *  Use "Simple Random" for weighted random rolls, or "Budget" for point-based selection. */
	UPROPERTY(EditAnywhere, Category = "RandomPool",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRandomPoolSelectionMode", ExcludeBaseStruct))
	FInstancedStruct SelectionMode;
};

namespace ArcCraftModifiers
{
	/** The rules' stat catalog entry for StatId; null (warns once) when Rules or its catalog is missing, or StatId is not in the catalog or has no attribute. */
	ARCCRAFT_API const FArcStatCatalogEntry* ResolveStat(const FArcCraftRulesData* Rules, FName StatId);

	/**
	 * The one conversion of a terminal modifier (Stats, Abilities, Effects, AttributeModifier, nested RandomPool) into
	 * pending results. Gates on MinR against Context.AverageR and on TriggerTags against IngredientTags; each result takes
	 * the SlotTags of the terminal modifier that produced it, EffectiveWeight and ModifierIndex are left to the caller.
	 * A stat draws one Context.Stream value; an unknown StatId or a missing catalog yields nothing and warns once.
	 * Game thread (resolves records).
	 */
	ARCCRAFT_API void ConvertModifier(
		const FInstancedStruct& Modifier,
		const FGameplayTagContainer& IngredientTags,
		const FArcCraftContext& Context,
		TArray<FArcCraftPendingModifier>& OutPending);
}
