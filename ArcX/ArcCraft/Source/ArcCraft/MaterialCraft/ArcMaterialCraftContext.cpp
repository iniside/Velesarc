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

#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemsHelpers.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

FArcMaterialCraftContext FArcMaterialCraftContext::Build(
	const TArray<const FArcItemData*>& ConsumedIngredients,
	const TArray<float>& QualityMultipliers,
	float InAverageQuality,
	const FGameplayTagContainer& InRecipeTags,
	int32 InBaseIngredientCount,
	float InExtraCraftTimeBonus)
{
	FArcMaterialCraftContext Context;

	// Build per-slot tags using FArcItemFragment_Tags::ItemTags (not GetItemAggregatedTags)
	Context.PerSlotTags.SetNum(ConsumedIngredients.Num());
	for (int32 Idx = 0; Idx < ConsumedIngredients.Num(); ++Idx)
	{
		if (const FArcItemData* Ingredient = ConsumedIngredients[Idx])
		{
			const FArcItemFragment_Tags* TagsFragment =
				ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(Ingredient);
			if (TagsFragment)
			{
				Context.PerSlotTags[Idx] = TagsFragment->ItemTags;
			}
		}
	}

	Context.RecipeTags = InRecipeTags;
	Context.IngredientQualityMultipliers = QualityMultipliers;
	Context.AverageQuality = InAverageQuality;
	Context.BandEligibilityQuality = InAverageQuality; // Adjusted by evaluator
	Context.BandWeightBonus = 0.0f; // Computed by evaluator
	Context.IngredientCount = ConsumedIngredients.Num();
	Context.BaseIngredientCount = InBaseIngredientCount > 0 ? InBaseIngredientCount : ConsumedIngredients.Num();
	Context.ExtraCraftTimeBonus = InExtraCraftTimeBonus;

	return Context;
}
