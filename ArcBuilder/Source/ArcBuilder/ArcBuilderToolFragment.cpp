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

#include "ArcBuilderToolFragment.h"

#include "ArcBuildingDefinition.h"
#include "Engine/AssetManager.h"

namespace ArcBuilderToolFragment_Local
{
	/** Resolve a UArcBuildingDefinition from a primary asset id (cached object, sync-load fallback). */
	UArcBuildingDefinition* ResolveBuildingDefinition(const FPrimaryAssetId& AssetId)
	{
		if (!AssetId.IsValid())
		{
			return nullptr;
		}

		UAssetManager& AssetManager = UAssetManager::Get();
		UArcBuildingDefinition* BuildingDef = AssetManager.GetPrimaryAssetObject<UArcBuildingDefinition>(AssetId);
		if (BuildingDef == nullptr)
		{
			const FSoftObjectPath AssetPath = AssetManager.GetPrimaryAssetPath(AssetId);
			if (AssetPath.IsValid())
			{
				BuildingDef = Cast<UArcBuildingDefinition>(AssetPath.TryLoad());
			}
		}
		return BuildingDef;
	}
}

namespace ArcBuilder
{
	void ResolveAvailableBuildings(const TArray<FArcNamedPrimaryAssetId>& Buildings,
	                               const FGameplayTagQuery& BuildingFilter,
	                               TArray<TObjectPtr<UArcBuildingDefinition>>& Out)
	{
		Out.Reset();

		// Explicit authored list.
		for (const FArcNamedPrimaryAssetId& Entry : Buildings)
		{
			if (UArcBuildingDefinition* Def = ArcBuilderToolFragment_Local::ResolveBuildingDefinition(Entry.AssetId))
			{
				Out.AddUnique(Def);
			}
		}

		// Tag-derived entries (skipped for an empty query).
		if (!BuildingFilter.IsEmpty())
		{
			UAssetManager& AssetManager = UAssetManager::Get();
			TArray<FPrimaryAssetId> BuildingIds;
			AssetManager.GetPrimaryAssetIdList(FPrimaryAssetType("ArcBuilding"), BuildingIds);

			for (const FPrimaryAssetId& AssetId : BuildingIds)
			{
				UArcBuildingDefinition* Def = ArcBuilderToolFragment_Local::ResolveBuildingDefinition(AssetId);
				if (Def != nullptr && BuildingFilter.Matches(Def->BuildingTags))
				{
					Out.AddUnique(Def);
				}
			}
		}
	}
}
