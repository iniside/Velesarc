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

#include "ArcCraft/Recipe/ArcCraftOutputBuilder.h"

#include "Items/ArcItemData.h"
#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"

FArcItemSpec FArcCraftOutputBuilder::Build(
	const UArcRecipeDefinition* Recipe,
	const TArray<const FArcItemData*>& MatchedIngredients,
	const TArray<float>& QualityMultipliers,
	float AverageQuality)
{
	FArcItemSpec OutputSpec;
	if (!Recipe || !Recipe->OutputItemDefinition.IsValid())
	{
		return OutputSpec;
	}

	// Compute output level
	uint8 OutputLevel = Recipe->OutputLevel;
	if (Recipe->bQualityAffectsLevel && !Recipe->QualityTierTable.IsNull())
	{
		UArcQualityTierTable* TierTable = Recipe->QualityTierTable.LoadSynchronous();
		if (TierTable)
		{
			float TotalTierValue = 0.0f;
			int32 TierCount = 0;
			for (int32 Idx = 0; Idx < MatchedIngredients.Num(); ++Idx)
			{
				if (MatchedIngredients[Idx])
				{
					const FGameplayTag BestTag = TierTable->FindBestTierTag(
						MatchedIngredients[Idx]->GetItemAggregatedTags());
					if (BestTag.IsValid())
					{
						TotalTierValue += TierTable->GetTierValue(BestTag);
						++TierCount;
					}
				}
			}
			if (TierCount > 0)
			{
				OutputLevel = static_cast<uint8>(FMath::Clamp(
					FMath::RoundToInt(TotalTierValue / TierCount), 1, 255));
			}
		}
	}

	// Create base output spec
	OutputSpec = FArcItemSpec::NewItem(Recipe->OutputItemDefinition, OutputLevel, Recipe->OutputAmount);

	// Apply output modifiers
	if (Recipe->ModifierSlots.Num() > 0)
	{
		// --- Slot-managed path ---
		// Phase 1: Evaluate all modifiers into pending results
		TArray<FArcCraftPendingModifier> PendingModifiers;
		PendingModifiers.Reserve(Recipe->OutputModifiers.Num());
		for (int32 Idx = 0; Idx < Recipe->OutputModifiers.Num(); ++Idx)
		{
			const FArcRecipeOutputModifier* Modifier =
				Recipe->OutputModifiers[Idx].GetPtr<FArcRecipeOutputModifier>();
			if (Modifier)
			{
				TArray<FArcCraftPendingModifier> Results = Modifier->Evaluate(
					OutputSpec, MatchedIngredients, QualityMultipliers, AverageQuality);
				for (FArcCraftPendingModifier& Pending : Results)
				{
					Pending.ModifierIndex = Idx;
				}
				PendingModifiers.Append(MoveTemp(Results));
			}
		}

		// Phase 2: Resolve through recipe-level slot configuration
		TArray<int32> ResolvedIndices = FArcCraftSlotResolver::Resolve(
			PendingModifiers, Recipe->ModifierSlots,
			Recipe->MaxUsableSlots, Recipe->SlotSelectionMode);

		// Phase 3: Apply only the resolved modifiers
		for (int32 Idx : ResolvedIndices)
		{
			if (PendingModifiers[Idx].ApplyFn)
			{
				PendingModifiers[Idx].ApplyFn(OutputSpec);
			}
		}
	}
	else
	{
		// --- Legacy direct-apply path ---
		for (const FInstancedStruct& ModifierStruct : Recipe->OutputModifiers)
		{
			const FArcRecipeOutputModifier* Modifier =
				ModifierStruct.GetPtr<FArcRecipeOutputModifier>();
			if (Modifier)
			{
				Modifier->ApplyToOutput(
					OutputSpec, MatchedIngredients, QualityMultipliers, AverageQuality);
			}
		}
	}

	return OutputSpec;
}

FArcItemSpec FArcCraftOutputBuilder::BuildFromSpecs(
	const UArcRecipeDefinition* Recipe,
	const TArray<FArcItemSpec>& InputItems)
{
	if (!Recipe)
	{
		return FArcItemSpec();
	}

	// When building from specs (entity path), we don't have live FArcItemData.
	// Use default quality (1.0) for all ingredient slots.
	const int32 NumIngredients = Recipe->Ingredients.Num();

	TArray<const FArcItemData*> MatchedIngredients;
	MatchedIngredients.SetNum(NumIngredients);

	TArray<float> QualityMultipliers;
	QualityMultipliers.SetNum(NumIngredients);

	for (int32 i = 0; i < NumIngredients; ++i)
	{
		MatchedIngredients[i] = nullptr;
		QualityMultipliers[i] = 1.0f;
	}

	// Compute average quality (all 1.0, but weighted by ingredient amounts for consistency)
	float TotalQuality = 0.0f;
	int32 TotalWeight = 0;
	for (int32 Idx = 0; Idx < NumIngredients; ++Idx)
	{
		const FArcRecipeIngredient* Ingredient = Recipe->GetIngredientBase(Idx);
		const int32 Weight = Ingredient ? Ingredient->Amount : 1;
		TotalQuality += QualityMultipliers[Idx] * Weight;
		TotalWeight += Weight;
	}
	const float AverageQuality = TotalWeight > 0 ? TotalQuality / TotalWeight : 1.0f;

	return Build(Recipe, MatchedIngredients, QualityMultipliers, AverageQuality);
}
