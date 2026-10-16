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
#include "Items/Fragments/ArcItemFragment.h"
#include "ArcNamedPrimaryAssetId.h"
#include "ArcBuilderToolFragment.generated.h"

/**
 * Item fragment carried by a "builder tool" item (e.g. a hammer) that turns the item
 * into a menu source for the build-mode ability (see ArcBuilderTask_BuildMode).
 *
 * The set of buildings offered while this tool is active is the union of:
 *  - Buildings: an explicit, authored list of building definitions.
 *  - all "ArcBuilding" primary assets whose UArcBuildingDefinition::BuildingTags
 *    match BuildingFilter (an empty query contributes no tag-derived entries).
 *
 * This struct is data only. The build-mode ability resolves the union via
 * ArcBuilder::ResolveAvailableBuildings and registers the result into
 * UArcBuilderSubsystem (RegisterTool) as an ephemeral cache.
 */
USTRUCT(BlueprintType)
struct FArcItemFragment_BuilderTool : public FArcItemFragment
{
	GENERATED_BODY()

public:
	/** Explicit list of building definitions always offered by this tool. */
	UPROPERTY(EditAnywhere, Category = "Builder", meta = (AllowedClasses = "/Script/ArcBuilder.ArcBuildingDefinition", DisplayThumbnail = false))
	TArray<FArcNamedPrimaryAssetId> Buildings;

	/** Additional buildings are offered when their BuildingTags match this query. Empty query = no tag-derived entries. */
	UPROPERTY(EditAnywhere, Category = "Builder")
	FGameplayTagQuery BuildingFilter;
};

class UArcBuildingDefinition;

namespace ArcBuilder
{
	/**
	 * Resolve the buildings offered by a builder source: the union of the explicit Buildings
	 * list and every "ArcBuilding" primary asset whose BuildingTags match BuildingFilter
	 * (tag pass skipped for an empty query). Sync-loads unresolved definitions.
	 */
	ARCBUILDER_API void ResolveAvailableBuildings(const TArray<FArcNamedPrimaryAssetId>& Buildings,
	                                              const FGameplayTagQuery& BuildingFilter,
	                                              TArray<TObjectPtr<UArcBuildingDefinition>>& Out);
}
