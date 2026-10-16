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

#include "ArcRandomPoolDefinitionData.generated.h"

/**
 * Tag-conditional weight modifier for a random pool entry.
 * When all RequiredTags are present in the aggregated ingredient tags,
 * BonusWeight is added and WeightMultiplier is applied to the entry's effective weight.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Tag-conditional weight modifier; adds BonusWeight and applies WeightMultiplier to a pool entry when all RequiredTags are present on ingredients."))
struct ARCCRAFT_API FArcRandomPoolWeightModifier
{
	GENERATED_BODY()

	/** Tags that must ALL be present on ingredients for this bonus to apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight")
	FGameplayTagContainer RequiredTags;

	/** Additive bonus to the entry's base weight when RequiredTags match. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight")
	float BonusWeight = 0.0f;

	/** Multiplicative factor applied to the entry's weight when RequiredTags match.
	 *  Applied after BonusWeight is added. 1.0 = no change. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight")
	float WeightMultiplier = 1.0f;
};

/**
 * A single entry in a random modifier pool.
 * Contains eligibility conditions, selection weight, budget cost,
 * and the actual output modifiers to apply.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "A single entry in a random modifier pool with eligibility conditions, selection weight, budget cost, and output modifiers."))
struct ARCCRAFT_API FArcRandomPoolEntry
{
	GENERATED_BODY()

	/** Display name for UI and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry")
	FText DisplayName;

	// ---- Eligibility ----

	/** Tags that must ALL be present on at least one ingredient for this entry to be eligible.
	 *  Checked against the aggregated tag container from all consumed ingredients. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Eligibility")
	FGameplayTagContainer RequiredIngredientTags;

	/** Tags that must NOT be present on any ingredient for this entry to be eligible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Eligibility")
	FGameplayTagContainer DenyIngredientTags;

	/** Minimum average R required for this entry to be eligible. 0 = always eligible. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Eligibility",
		meta = (ClampMin = "0.0"))
	float MinR = 0.0f;

	// ---- Weight ----

	/** Base selection weight. Higher = more likely to be selected. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Weight",
		meta = (ClampMin = "0.001"))
	float BaseWeight = 1.0f;

	/** How much the average R scales the effective weight.
	 *  Formula: EffWeight *= (1 + (R - 1) * RWeightScaling)
	 *  0 = R has no effect on weight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Weight")
	float RWeightScaling = 0.0f;

	/** Tag-conditional weight modifiers. Multiple can fire simultaneously. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Weight")
	TArray<FArcRandomPoolWeightModifier> WeightModifiers;

	// ---- Budget ----

	/** Cost deducted from the budget when this entry is selected (budget mode only).
	 *  0 = free (always selected if eligible). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Budget",
		meta = (ClampMin = "0.0"))
	float Cost = 1.0f;

	// ---- Modifiers ----

	/** Terminal modifiers to apply when this entry is selected.
	 *  Only Stats, Abilities, and Effects modifiers are allowed here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry|Modifiers",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftModifier", ExcludeBaseStruct))
	TArray<FInstancedStruct> Modifiers;
};

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcRandomPoolDefinitionData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pool")
	FGuid PoolId;

	/** Display name for UI and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	FText PoolName;

	/** All possible entries in this pool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pool")
	TArray<FArcRandomPoolEntry> Entries;
};
