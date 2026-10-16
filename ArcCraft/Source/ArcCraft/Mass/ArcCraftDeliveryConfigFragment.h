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
#include "StructUtils/InstancedStruct.h"

#include "ArcCraftDeliveryConfigFragment.generated.h"

/**
 * Const-shared fragment pairing an output delivery policy with an ingredient source
 * policy for a crafting station entity.
 *
 * Kept separate from FArcCraftStationConfigFragment so delivery policies hash into
 * their own shared-fragment key — stations with identical policies share one fragment
 * even when their station configs differ. Seeded by UArcMassCraftingStationTrait.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Const-shared fragment pairing an output delivery policy with an ingredient source policy for a crafting station entity."))
struct ARCCRAFT_API FArcCraftDeliveryConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	/** How crafted items are delivered (e.g. entity store, player item store). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftOutputDelivery"))
	FInstancedStruct OutputDelivery;

	/** Where ingredients are sourced from (e.g. instigator item store, entity store). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting",
		meta = (BaseStruct = "/Script/ArcCraft.ArcCraftItemSource"))
	FInstancedStruct IngredientSource;
};

template <>
struct TMassFragmentTraits<FArcCraftDeliveryConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
