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
#include "Templates/Function.h"
#include "UObject/PrimaryAssetId.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"

struct FArcCraftRulesData;
struct FArcItemDefinitionData;
struct FArcItemSpec;
struct FArcRecipeDefinitionData;

struct FArcCraftSlotCandidate
{
	FPrimaryAssetId DefinitionId;

	FGameplayTagContainer InstanceTags;

	float Step = 1.0f;

	int32 Amount = 0;
};

struct FArcCraftSlotTake
{
	int32 CandidateIndex = INDEX_NONE;

	int32 Amount = 0;
};

namespace ArcCraftRarity
{
	/** Game thread; null when the settings id is unset or the record is not indexed. */
	ARCCRAFT_API const FArcCraftRulesData* GetRules();

	/** Ladder step of the spec's rarity; no rarity means DefaultRarity. 1 (Common) when the rules are null or the rarity is not on the ladder. Game thread. */
	ARCCRAFT_API float GetRarityStep(const FArcItemSpec& Spec);

	/** Candidates group by (DefinitionId, InstanceTags); rarity may differ inside a group. The first group (in candidate order) whose candidates together cover max(0, Required) wins; its candidates are taken in order up to Required. OutStep = sum(Step*Take)/Required, or the first taken Step when Required is 0. False when no group covers Required. */
	ARCCRAFT_API bool ComputeSlotDraw(TConstArrayView<FArcCraftSlotCandidate> Candidates, int32 Required, TArray<FArcCraftSlotTake>& OutTakes, float& OutStep);

	/** Average of the slot steps weighted by each ingredient's Amount; a slot false in SlotFilled weighs 0, a missing slot step counts as 1, a missing SlotFilled entry as filled. */
	ARCCRAFT_API float ComputeAverageR(const FArcRecipeDefinitionData& Recipe, TConstArrayView<float> SlotRaritySteps, TConstArrayView<bool> SlotFilled);

	/** Band = Lerp(Min, Max, [BandMinFraction, BandMaxFraction]) of OutputRarity's ladder entry, the full range when Rules is null or OutputRarity is empty or not on the ladder; returns Lerp(Band, U^(1/max(1e-4, 1 + BiasPerR*(R-1) + BiasPerSkillMargin*SkillMargin))). */
	ARCCRAFT_API float ComputeBandValue(
		float Min,
		float Max,
		const FArcCraftRulesData* Rules,
		const FGameplayTag& OutputRarity,
		float R,
		float BiasPerR,
		float BiasPerSkillMargin,
		float SkillMargin,
		float U);

	/** False when the definition is null or its AssetTags hold any of the rules' NoRarityOutputTags. */
	ARCCRAFT_API bool DoesOutputRollRarity(const FArcCraftRulesData& Rules, const FArcItemDefinitionData* OutputDefinition);

	/** Ladder is ordered lowest to highest and Ladder[0] is Poor; Draw returns a uniform value in [0, 1]. */
	ARCCRAFT_API FGameplayTag DecideOutputRarity(
		const FArcCraftRulesData& Rules,
		const FGameplayTag& GuaranteedRarity,
		float AverageR,
		float SkillMargin,
		TFunctionRef<float()> Draw);
}
