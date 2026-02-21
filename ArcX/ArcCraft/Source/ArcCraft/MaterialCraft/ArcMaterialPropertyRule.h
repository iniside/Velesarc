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
#include "StructUtils/InstancedStruct.h"

#include "ArcMaterialPropertyRule.generated.h"

class UArcQualityBandPreset;

/**
 * A quality band defines a range of quality values and the modifiers that apply when selected.
 * Multiple bands can be eligible for a given quality -- selection is weighted random,
 * so higher quality makes better bands more likely but doesn't guarantee them.
 *
 * Effective weight formula:
 *   BaseWeight * (1 + (max(0, BandEligibilityQuality - MinQuality) + BandWeightBonus) * QualityWeightBias)
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcMaterialQualityBand
{
	GENERATED_BODY()

public:
	/** Display name for editor and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band")
	FText BandName;

	/** Minimum quality value to be eligible for this band (inclusive).
	 *  Quality below this threshold means this band cannot be selected.
	 *  Compared against BandEligibilityQuality (NOT boosted by extra same-quality items). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band", meta = (ClampMin = "0.0"))
	float MinQuality = 0.0f;

	/** Base selection weight. Higher = more likely to be selected among eligible bands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band|Selection", meta = (ClampMin = "0.001"))
	float BaseWeight = 1.0f;

	/** How much quality above MinQuality (plus BandWeightBonus) boosts this band's selection weight.
	 *  Formula: EffectiveWeight = BaseWeight * (1 + (max(0, Quality - MinQuality) + BandWeightBonus) * QualityWeightBias)
	 *  0.0 = quality has no effect on weight. Higher values make this band increasingly
	 *  likely when quality exceeds MinQuality or when extra ingredients add weight bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band|Selection")
	float QualityWeightBias = 0.0f;

	/** Modifiers to apply when this band is selected.
	 *  Uses the same instanced struct pattern as recipe OutputModifiers.
	 *  Typically contains FArcRecipeOutputModifier_Stats, _Abilities, _Effects, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Band|Modifiers",
		meta = (BaseStruct = "/Script/ArcCraft.ArcRecipeOutputModifier", ExcludeBaseStruct))
	TArray<FInstancedStruct> Modifiers;
};

/**
 * Maps a tag pattern to quality bands.
 * When the per-slot ingredient tags match the TagQuery, this rule's bands
 * become eligible for selection.
 *
 * Supports full boolean tag expressions via FGameplayTagQuery:
 *   - "Resource.Metal" -- fires if any ingredient slot has metal tags
 *   - "Resource.Metal AND Resource.Gem" -- fires if metal + gem tags exist across slots
 *   - "Resource.Metal.Iron OR Resource.Metal.Steel" -- fires for either
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcMaterialPropertyRule
{
	GENERATED_BODY()

public:
	/** Display name for editor and debugging. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule")
	FText RuleName;

	/** Tag query that must match against the combined per-slot ingredient tags.
	 *  Supports AND, OR, NOT, and hierarchical tag matching. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Matching")
	FGameplayTagQuery TagQuery;

	/** If non-empty, the rule only fires if the recipe has ALL of these tags.
	 *  Allows rules to be scoped to specific recipe families
	 *  (e.g., "Recipe.Weapon.Sword"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Matching")
	FGameplayTagContainer RequiredRecipeTags;

	/** Priority for rule ordering. Higher priority rules are evaluated first.
	 *  When MaxActiveRules is set on the table, higher priority rules take precedence. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Matching")
	int32 Priority = 0;

	/** How many times this rule can contribute to the output per craft.
	 *  For tag combos you typically want 1. For "per-instance" bonuses, higher. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Selection", meta = (ClampMin = 1))
	int32 MaxContributions = 1;

	/** Quality bands defined inline on this rule.
	 *  Ignored if QualityBandPreset is set and loadable.
	 *  Use GetEffectiveQualityBands() to resolve the active bands. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Bands")
	TArray<FArcMaterialQualityBand> QualityBands;

	/** Optional shared quality band preset. If set, overrides inline QualityBands.
	 *  Use this to share identical band definitions across multiple rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Bands")
	TSoftObjectPtr<UArcQualityBandPreset> QualityBandPreset;

	/** Tags added to the output item when this rule fires.
	 *  Used for tracking which material properties were applied (for UI, post-craft, etc.)
	 *  and for routing evaluations to modifier slot configs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rule|Output")
	FGameplayTagContainer OutputTags;

	/** Get the effective quality bands for this rule.
	 *  Returns bands from QualityBandPreset if set and loadable, otherwise inline QualityBands. */
	const TArray<FArcMaterialQualityBand>& GetEffectiveQualityBands() const;
};
