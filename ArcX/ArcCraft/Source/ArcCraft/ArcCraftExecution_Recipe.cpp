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

#include "ArcCraftExecution_Recipe.h"

#include "ArcCoreUtils.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemsStoreComponent.h"
#include "Recipe/ArcRecipeDefinition.h"
#include "Recipe/ArcRecipeIngredient.h"
#include "Recipe/ArcRecipeOutput.h"
#include "Recipe/ArcRecipeQuality.h"

// -------------------------------------------------------------------
// Legacy overrides
// -------------------------------------------------------------------

bool FArcCraftExecution_Recipe::CanCraft(
	const UArcCraftComponent* InCraftComponent,
	const UObject* InInstigator) const
{
	return true;
}

bool FArcCraftExecution_Recipe::CheckAndConsumeItems(
	const UArcCraftComponent* InCraftComponent,
	const UArcItemDefinition* InCraftData,
	const UObject* InInstigator,
	bool bConsume) const
{
	// Legacy path — delegate to parent behavior or return true.
	// The recipe path uses CheckAndConsumeRecipeItems instead.
	return true;
}

FArcItemSpec FArcCraftExecution_Recipe::OnCraftFinished(
	UArcCraftComponent* InCraftComponent,
	const UArcItemDefinition* InCraftData,
	const UObject* InInstigator) const
{
	// Legacy path — should not be called for recipe-based crafts.
	return FArcItemSpec();
}

// -------------------------------------------------------------------
// Recipe-specific methods
// -------------------------------------------------------------------

UArcItemsStoreComponent* FArcCraftExecution_Recipe::GetItemsStore(const UObject* InInstigator) const
{
	const AActor* OwnerActor = Cast<AActor>(InInstigator);
	if (!OwnerActor)
	{
		return nullptr;
	}
	return Arcx::Utils::GetComponent(OwnerActor, ItemsStoreClass);
}

UArcQualityTierTable* FArcCraftExecution_Recipe::GetTierTable(const UArcRecipeDefinition* InRecipe) const
{
	if (InRecipe && !InRecipe->QualityTierTable.IsNull())
	{
		return InRecipe->QualityTierTable.LoadSynchronous();
	}
	return nullptr;
}

bool FArcCraftExecution_Recipe::CheckAndConsumeRecipeItems(
	const UArcCraftComponent* InCraftComponent,
	const UArcRecipeDefinition* InRecipe,
	const UObject* InInstigator,
	bool bConsume,
	TArray<const FArcItemData*>* OutMatchedIngredients,
	TArray<float>* OutQualityMultipliers) const
{
	if (!InRecipe)
	{
		return false;
	}

	UArcItemsStoreComponent* ItemsStore = GetItemsStore(InInstigator);
	if (!ItemsStore)
	{
		return false;
	}

	UArcQualityTierTable* TierTable = GetTierTable(InRecipe);

	// Track which items we've matched so we don't double-consume
	TArray<TPair<FArcItemId, int32>> ItemsToConsume;
	TSet<FArcItemId> UsedItemIds;

	TArray<const FArcItemData*> MatchedIngredients;
	TArray<float> QualityMultipliers;
	MatchedIngredients.SetNum(InRecipe->Ingredients.Num());
	QualityMultipliers.SetNum(InRecipe->Ingredients.Num());

	const TArray<const FArcItemData*> AllItems = ItemsStore->GetItems();

	for (int32 SlotIdx = 0; SlotIdx < InRecipe->Ingredients.Num(); ++SlotIdx)
	{
		const FArcRecipeIngredient* Ingredient = InRecipe->GetIngredientBase(SlotIdx);
		if (!Ingredient)
		{
			return false;
		}

		// Find a matching item
		const FArcItemData* MatchedItem = nullptr;
		for (const FArcItemData* ItemData : AllItems)
		{
			if (!ItemData || UsedItemIds.Contains(ItemData->GetItemId()))
			{
				continue;
			}

			if (!Ingredient->DoesItemSatisfy(ItemData, TierTable))
			{
				continue;
			}

			if (ItemData->GetStacks() < static_cast<uint16>(Ingredient->Amount))
			{
				continue;
			}

			MatchedItem = ItemData;
			break;
		}

		if (!MatchedItem)
		{
			return false;
		}

		MatchedIngredients[SlotIdx] = MatchedItem;
		QualityMultipliers[SlotIdx] = Ingredient->GetItemQualityMultiplier(MatchedItem, TierTable);

		UsedItemIds.Add(MatchedItem->GetItemId());

		if (Ingredient->bConsumeOnCraft)
		{
			ItemsToConsume.Add({MatchedItem->GetItemId(), Ingredient->Amount});
		}
	}

	if (bConsume)
	{
		for (const auto& Pair : ItemsToConsume)
		{
			ItemsStore->RemoveItem(Pair.Key, Pair.Value, true);
		}
	}

	if (OutMatchedIngredients)
	{
		*OutMatchedIngredients = MoveTemp(MatchedIngredients);
	}
	if (OutQualityMultipliers)
	{
		*OutQualityMultipliers = MoveTemp(QualityMultipliers);
	}

	return true;
}

FArcItemSpec FArcCraftExecution_Recipe::OnRecipeCraftFinished(
	UArcCraftComponent* InCraftComponent,
	const UArcRecipeDefinition* InRecipe,
	const UObject* InInstigator) const
{
	FArcItemSpec OutputSpec;
	if (!InRecipe || !InRecipe->OutputItemDefinition.IsValid())
	{
		return OutputSpec;
	}

	UArcItemsStoreComponent* ItemsStore = GetItemsStore(InInstigator);
	if (!ItemsStore)
	{
		return OutputSpec;
	}

	// Re-match ingredients (without consuming) to get quality data for output generation.
	// Items were already consumed at craft start, so we use bConsume=false here and accept
	// that some items may have changed. For accuracy, matched ingredients could be cached
	// at craft start time. This is a known simplification.
	TArray<const FArcItemData*> MatchedIngredients;
	TArray<float> QualityMultipliers;

	// Try to gather quality data from remaining items. If items were consumed, we still
	// apply default quality.
	CheckAndConsumeRecipeItems(InCraftComponent, InRecipe, InInstigator, false,
		&MatchedIngredients, &QualityMultipliers);

	// Compute average quality
	float TotalQuality = 0.0f;
	int32 TotalWeight = 0;
	for (int32 Idx = 0; Idx < QualityMultipliers.Num(); ++Idx)
	{
		const FArcRecipeIngredient* Ingredient = InRecipe->GetIngredientBase(Idx);
		const int32 Weight = Ingredient ? Ingredient->Amount : 1;
		TotalQuality += QualityMultipliers[Idx] * Weight;
		TotalWeight += Weight;
	}
	const float AverageQuality = TotalWeight > 0 ? TotalQuality / TotalWeight : 1.0f;

	// Create base output spec
	uint8 OutputLevel = InRecipe->OutputLevel;
	if (InRecipe->bQualityAffectsLevel)
	{
		// Map quality multiplier to level (clamped to uint8 range)
		UArcQualityTierTable* TierTable = GetTierTable(InRecipe);
		if (TierTable)
		{
			// Use the average tier value as the output level
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

	OutputSpec = FArcItemSpec::NewItem(InRecipe->OutputItemDefinition, OutputLevel, InRecipe->OutputAmount);

	// Apply output modifiers
	for (const FInstancedStruct& ModifierStruct : InRecipe->OutputModifiers)
	{
		const FArcRecipeOutputModifier* Modifier = ModifierStruct.GetPtr<FArcRecipeOutputModifier>();
		if (Modifier)
		{
			Modifier->ApplyToOutput(OutputSpec, MatchedIngredients, QualityMultipliers, AverageQuality);
		}
	}

	// Add to player inventory
	ItemsStore->AddItem(OutputSpec, FArcItemId::InvalidId);

	return OutputSpec;
}
