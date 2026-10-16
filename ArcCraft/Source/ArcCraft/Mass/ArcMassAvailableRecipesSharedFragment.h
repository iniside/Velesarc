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

#pragma once

#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcMassAvailableRecipesSharedFragment.generated.h"

/**
 * Const-shared fragment listing the recipes available on a crafting station entity.
 * Deduplicates across all entities sharing the same recipe set.
 * Seeded once by UArcMassCraftingStationTrait::BuildTemplate; immutable at runtime.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Const-shared fragment listing the recipes available on a crafting station entity; seeded once by UArcMassCraftingStationTrait and immutable at runtime."))
struct ARCCRAFT_API FArcMassAvailableRecipesSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** Recipe record ids available on this crafting station. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting")
	TArray<FPrimaryAssetId> Recipes;
};

template <>
struct TMassFragmentTraits<FArcMassAvailableRecipesSharedFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
