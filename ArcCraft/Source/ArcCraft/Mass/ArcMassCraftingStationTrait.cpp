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

#include "ArcCraft/Mass/ArcMassCraftingStationTrait.h"

#include "MassEntityTemplateRegistry.h"
#include "Mass/EntityUtils.h"
#include "MassWorldUtils.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcMassAvailableRecipesSharedFragment.h"
#include "ArcCraft/Mass/ArcCraftDeliveryConfigFragment.h"

void UArcMassCraftingStationTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// Regular fragments required by UArcCraftTickProcessor's query
	BuildContext.AddFragment<FArcCraftQueueFragment>();
	BuildContext.AddFragment<FArcCraftInputFragment>();
	BuildContext.AddFragment<FArcCraftOutputFragment>();

	// Station identification tags
	BuildContext.AddTag<FArcCraftStationTag>();
	if (bAutoTick)
	{
		BuildContext.AddTag<FArcCraftAutoTickTag>();
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	// Const-shared fragment: available recipe list (drives UI recipe panel + server validation)
	FArcMassAvailableRecipesSharedFragment RecipesFragment;
	RecipesFragment.Recipes.Reserve(AvailableRecipes.Num());
	for (const FArcNamedPrimaryAssetId& RecipeId : AvailableRecipes)
	{
		RecipesFragment.Recipes.Add(RecipeId.AssetId);
	}
	const FConstSharedStruct RecipesShared = EntityManager.GetOrCreateConstSharedFragment(RecipesFragment);
	BuildContext.AddConstSharedFragment(RecipesShared);

	// Const-shared fragment: delivery policy + ingredient source policy
	FArcCraftDeliveryConfigFragment DeliveryConfig;
	DeliveryConfig.OutputDelivery = OutputDelivery;
	DeliveryConfig.IngredientSource = IngredientSource;
	const FConstSharedStruct DeliveryShared = EntityManager.GetOrCreateConstSharedFragment(DeliveryConfig);
	BuildContext.AddConstSharedFragment(DeliveryShared);

	// Const-shared fragment: station config (time mode, tick interval, queue size, tags)
	FArcCraftStationConfigFragment StationConfig;
	StationConfig.StationTags = StationTags;
	StationConfig.OfferedRecipeTags = OfferedRecipeTags;
	StationConfig.TimeMode = TimeMode;
	StationConfig.TickInterval = TickInterval;
	StationConfig.ConcurrentCraftSlots = ConcurrentCraftSlots;
	StationConfig.QueueLengthPerSlot = QueueLengthPerSlot;
	const FConstSharedStruct StationShared = EntityManager.GetOrCreateConstSharedFragment(StationConfig);
	BuildContext.AddConstSharedFragment(StationShared);
}
