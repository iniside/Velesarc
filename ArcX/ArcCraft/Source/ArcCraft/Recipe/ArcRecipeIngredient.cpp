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

#include "ArcCraft/Recipe/ArcRecipeIngredient.h"

#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "Items/ArcItemsHelpers.h"

// -------------------------------------------------------------------
// FArcRecipeIngredient (base)
// -------------------------------------------------------------------

bool FArcRecipeIngredient::DoesItemSatisfy(
	const FArcItemData* InItemData,
	const UArcQualityTierTable* InTierTable) const
{
	return InItemData != nullptr;
}

float FArcRecipeIngredient::GetItemQualityMultiplier(
	const FArcItemData* InItemData,
	const UArcQualityTierTable* InTierTable) const
{
	return 1.0f;
}

// -------------------------------------------------------------------
// FArcRecipeIngredient_ItemDef
// -------------------------------------------------------------------

bool FArcRecipeIngredient_ItemDef::DoesItemSatisfy(
	const FArcItemData* InItemData,
	const UArcQualityTierTable* InTierTable) const
{
	if (!InItemData)
	{
		return false;
	}

	return InItemData->GetItemDefinitionId() == ItemDefinitionId;
}

float FArcRecipeIngredient_ItemDef::GetItemQualityMultiplier(
	const FArcItemData* InItemData,
	const UArcQualityTierTable* InTierTable) const
{
	if (!InItemData || !InTierTable)
	{
		return 1.0f;
	}

	const FGameplayTagContainer& ItemTags = InItemData->GetItemAggregatedTags();
	return InTierTable->EvaluateQuality(ItemTags);
}

// -------------------------------------------------------------------
// FArcRecipeIngredient_Tags
// -------------------------------------------------------------------

bool FArcRecipeIngredient_Tags::DoesItemSatisfy(
	const FArcItemData* InItemData,
	const UArcQualityTierTable* InTierTable) const
{
	if (!InItemData)
	{
		return false;
	}

	const FArcItemFragment_Tags* TagsFragment = ArcItemsHelper::GetFragment<FArcItemFragment_Tags>(InItemData);
	if (!TagsFragment)
	{
		return false;
	}
	
	const FGameplayTagContainer& ItemTags = TagsFragment->AssetTags;	
	// Check required tags
	if (RequiredTags.Num() > 0 && !ItemTags.HasAll(RequiredTags))
	{
		return false;
	}

	// Check deny tags
	if (DenyTags.Num() > 0 && ItemTags.HasAny(DenyTags))
	{
		return false;
	}

	// Check minimum tier
	if (MinimumTierTag.IsValid() && InTierTable)
	{
		const int32 MinTierValue = InTierTable->GetTierValue(MinimumTierTag);
		const FGameplayTag BestTag = InTierTable->FindBestTierTag(ItemTags);

		if (!BestTag.IsValid())
		{
			return false;
		}

		const int32 ItemTierValue = InTierTable->GetTierValue(BestTag);
		if (ItemTierValue < MinTierValue)
		{
			return false;
		}
	}

	return true;
}

float FArcRecipeIngredient_Tags::GetItemQualityMultiplier(
	const FArcItemData* InItemData,
	const UArcQualityTierTable* InTierTable) const
{
	if (!InItemData || !InTierTable)
	{
		return 1.0f;
	}

	const FGameplayTagContainer& ItemTags = InItemData->GetItemAggregatedTags();
	return InTierTable->EvaluateQuality(ItemTags);
}
