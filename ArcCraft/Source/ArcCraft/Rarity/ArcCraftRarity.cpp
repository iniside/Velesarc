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

#include "ArcCraft/Rarity/ArcCraftRarity.h"

#include "ArcCraft/Rarity/ArcCraftRulesData.h"
#include "ArcCraft/Rarity/ArcCraftSettings.h"
#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/Fragments/ArcItemFragment_Rarity.h"

namespace ArcCraftRarityLocal
{
	constexpr float NeutralStep = 1.0f;

	constexpr float MinBandExponentBase = 1e-4f;

	int32 FindLadderIndex(const FArcCraftRulesData& Rules, const FGameplayTag& Rarity)
	{
		return Rules.Ladder.IndexOfByPredicate(
			[&Rarity](const FArcCraftRarityStep& Candidate) -> bool
			{
				return Candidate.Rarity == Rarity;
			});
	}

	const FArcCraftRarityStep* FindLadderStep(const FArcCraftRulesData& Rules, const FGameplayTag& Rarity)
	{
		const int32 LadderIndex = FindLadderIndex(Rules, Rarity);
		return LadderIndex != INDEX_NONE ? &Rules.Ladder[LadderIndex] : nullptr;
	}

	bool IsSameSlotGroup(const FArcCraftSlotCandidate& A, const FArcCraftSlotCandidate& B)
	{
		return A.DefinitionId == B.DefinitionId && A.InstanceTags == B.InstanceTags;
	}
}

const FArcCraftRulesData* ArcCraftRarity::GetRules()
{
	return ArcCraftRecords::Find<FArcCraftRulesData>(GetDefault<UArcCraftSettings>()->Rules.AssetId);
}

float ArcCraftRarity::GetRarityStep(const FArcItemSpec& Spec)
{
	const FArcCraftRulesData* Rules = GetRules();
	if (Rules == nullptr)
	{
		return ArcCraftRarityLocal::NeutralStep;
	}

	FGameplayTag Rarity = ArcMassItems::GetSpecRarity(Spec);
	if (!Rarity.IsValid())
	{
		Rarity = Rules->DefaultRarity;
	}

	const FArcCraftRarityStep* LadderStep = ArcCraftRarityLocal::FindLadderStep(*Rules, Rarity);
	return LadderStep != nullptr ? LadderStep->Step : ArcCraftRarityLocal::NeutralStep;
}

bool ArcCraftRarity::ComputeSlotDraw(TConstArrayView<FArcCraftSlotCandidate> Candidates, int32 Required, TArray<FArcCraftSlotTake>& OutTakes, float& OutStep)
{
	const int32 ClampedRequired = FMath::Max(0, Required);
	for (int32 CandidateIdx = 0; CandidateIdx < Candidates.Num(); ++CandidateIdx)
	{
		const FArcCraftSlotCandidate& Candidate = Candidates[CandidateIdx];
		const bool bGroupTried = Candidates.Left(CandidateIdx).ContainsByPredicate(
			[&Candidate](const FArcCraftSlotCandidate& Earlier) -> bool
			{
				return ArcCraftRarityLocal::IsSameSlotGroup(Earlier, Candidate);
			});
		if (bGroupTried)
		{
			continue;
		}

		int32 Total = 0;
		for (const FArcCraftSlotCandidate& Other : Candidates)
		{
			if (ArcCraftRarityLocal::IsSameSlotGroup(Other, Candidate))
			{
				Total += Other.Amount;
			}
		}
		if (Total < ClampedRequired)
		{
			continue;
		}

		OutTakes.Reset();
		float WeightedSteps = 0.0f;
		int32 Remaining = ClampedRequired;
		for (int32 OtherIdx = 0; OtherIdx < Candidates.Num(); ++OtherIdx)
		{
			const FArcCraftSlotCandidate& Other = Candidates[OtherIdx];
			if (!ArcCraftRarityLocal::IsSameSlotGroup(Other, Candidate))
			{
				continue;
			}
			if (Remaining <= 0 && !OutTakes.IsEmpty())
			{
				break;
			}

			const int32 Take = FMath::Min(Remaining, Other.Amount);
			FArcCraftSlotTake& SlotTake = OutTakes.AddDefaulted_GetRef();
			SlotTake.CandidateIndex = OtherIdx;
			SlotTake.Amount = Take;
			WeightedSteps += Other.Step * static_cast<float>(Take);
			Remaining -= Take;
			if (ClampedRequired == 0)
			{
				OutStep = Other.Step;
			}
		}

		if (ClampedRequired > 0)
		{
			OutStep = WeightedSteps / static_cast<float>(ClampedRequired);
		}
		return true;
	}
	return false;
}

float ArcCraftRarity::ComputeAverageR(const FArcRecipeDefinitionData& Recipe, TConstArrayView<float> SlotRaritySteps, TConstArrayView<bool> SlotFilled)
{
	const int32 NumIngredients = Recipe.Ingredients.Num();
	if (NumIngredients == 0 || SlotRaritySteps.IsEmpty())
	{
		return ArcCraftRarityLocal::NeutralStep;
	}

	float WeightedSum = 0.0f;
	int32 TotalWeight = 0;
	for (int32 Idx = 0; Idx < NumIngredients; ++Idx)
	{
		if (SlotFilled.IsValidIndex(Idx) && !SlotFilled[Idx])
		{
			continue;
		}
		const FArcRecipeIngredient* Ingredient = Recipe.GetIngredientBase(Idx);
		const int32 Weight = Ingredient != nullptr ? Ingredient->Amount : 1;
		const float Step = SlotRaritySteps.IsValidIndex(Idx) ? SlotRaritySteps[Idx] : ArcCraftRarityLocal::NeutralStep;
		WeightedSum += Step * static_cast<float>(Weight);
		TotalWeight += Weight;
	}

	return TotalWeight > 0 ? WeightedSum / static_cast<float>(TotalWeight) : ArcCraftRarityLocal::NeutralStep;
}

float ArcCraftRarity::ComputeBandValue(
	float Min,
	float Max,
	const FArcCraftRulesData* Rules,
	const FGameplayTag& OutputRarity,
	float R,
	float BiasPerR,
	float BiasPerSkillMargin,
	float SkillMargin,
	float U)
{
	float MinFraction = 0.0f;
	float MaxFraction = 1.0f;
	if (Rules != nullptr && OutputRarity.IsValid())
	{
		if (const FArcCraftRarityStep* LadderStep = ArcCraftRarityLocal::FindLadderStep(*Rules, OutputRarity))
		{
			MinFraction = LadderStep->BandMinFraction;
			MaxFraction = LadderStep->BandMaxFraction;
		}
	}

	const float BandMin = FMath::Lerp(Min, Max, MinFraction);
	const float BandMax = FMath::Lerp(Min, Max, MaxFraction);
	const float ExponentBase = 1.0f + BiasPerR * (R - ArcCraftRarityLocal::NeutralStep) + BiasPerSkillMargin * SkillMargin;
	const float Exponent = 1.0f / FMath::Max(ArcCraftRarityLocal::MinBandExponentBase, ExponentBase);
	return FMath::Lerp(BandMin, BandMax, FMath::Pow(U, Exponent));
}

bool ArcCraftRarity::DoesOutputRollRarity(const FArcCraftRulesData& Rules, const FArcItemDefinitionData* OutputDefinition)
{
	if (OutputDefinition == nullptr)
	{
		return false;
	}

	const FArcItemFragment_Tags* TagsFragment = OutputDefinition->FindFragment<FArcItemFragment_Tags>();
	return TagsFragment == nullptr || !TagsFragment->AssetTags.HasAny(Rules.NoRarityOutputTags);
}

FGameplayTag ArcCraftRarity::DecideOutputRarity(
	const FArcCraftRulesData& Rules,
	const FGameplayTag& GuaranteedRarity,
	float AverageR,
	float SkillMargin,
	TFunctionRef<float()> Draw)
{
	FGameplayTag StartRarity = Rules.DefaultRarity;
	if (GuaranteedRarity.IsValid())
	{
		StartRarity = GuaranteedRarity;
	}
	else if (!Rules.Ladder.IsEmpty() && AverageR < Rules.PoorBelowR)
	{
		StartRarity = Rules.Ladder[0].Rarity;
	}

	int32 LadderIndex = ArcCraftRarityLocal::FindLadderIndex(Rules, StartRarity);
	if (LadderIndex == INDEX_NONE)
	{
		return StartRarity;
	}

	const FArcCraftUpgradeRoll& Roll = Rules.UpgradeRoll;
	float StepChance = FMath::Clamp(
		Roll.Base + Roll.PerSkillMargin * SkillMargin + Roll.PerR * (AverageR - ArcCraftRarityLocal::NeutralStep),
		0.0f,
		Roll.Cap);
	while (LadderIndex + 1 < Rules.Ladder.Num() && Draw() < StepChance)
	{
		++LadderIndex;
		StepChance = Roll.NextStepChance;
	}

	return Rules.Ladder[LadderIndex].Rarity;
}
