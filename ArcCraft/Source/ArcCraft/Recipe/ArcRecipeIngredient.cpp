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

#include "Items/ArcItemSpec.h"
#include "Items/ArcItemDefinitionData.h"
#include "ArcCraft/Rarity/ArcCraftRarity.h"
#include "ArcCraft/Shared/ArcCraftItemTags.h"

// -------------------------------------------------------------------
// FArcRecipeIngredient (base)
// -------------------------------------------------------------------

bool FArcRecipeIngredient::DoesItemSatisfy(const FArcItemSpec& InItemSpec) const
{
	return InItemSpec.GetItemDefinitionId().IsValid();
}

float FArcRecipeIngredient::GetItemRarityStep(const FArcItemSpec& InItemSpec) const
{
	return ArcCraftRarity::GetRarityStep(InItemSpec);
}

bool FArcRecipeIngredient::MeetsMinRarity(const FArcItemSpec& InItemSpec) const
{
	if (MinRarityStep <= 0.0f)
	{
		return true;
	}

	return ArcCraftRarity::GetRarityStep(InItemSpec) >= MinRarityStep;
}

// -------------------------------------------------------------------
// FArcRecipeIngredient_ItemDef
// -------------------------------------------------------------------

bool FArcRecipeIngredient_ItemDef::DoesItemSatisfy(const FArcItemSpec& InItemSpec) const
{
	if (!InItemSpec.GetItemDefinitionId().IsValid())
	{
		return false;
	}

	if (InItemSpec.GetItemDefinitionId() != ItemDefinitionId)
	{
		return false;
	}

	if (DenyTags.Num() > 0 && ArcCraftItemTags::GetCraftTags(InItemSpec).HasAny(DenyTags))
	{
		return false;
	}

	return MeetsMinRarity(InItemSpec);
}

// -------------------------------------------------------------------
// FArcRecipeIngredient_Tags
// -------------------------------------------------------------------

bool FArcRecipeIngredient_Tags::DoesItemSatisfy(const FArcItemSpec& InItemSpec) const
{
	if (InItemSpec.GetItemDefinition() == nullptr)
	{
		return false;
	}

	const FGameplayTagContainer ItemTags = ArcCraftItemTags::GetCraftTags(InItemSpec);
	if (RequiredTags.Num() > 0 && !ItemTags.HasAll(RequiredTags))
	{
		return false;
	}

	if (DenyTags.Num() > 0 && ItemTags.HasAny(DenyTags))
	{
		return false;
	}

	return MeetsMinRarity(InItemSpec);
}
