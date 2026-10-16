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

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "GameplayTagContainer.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"

#include "ArcStatTransferEntry.generated.h"

USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcStatTransferEntry
{
	GENERATED_BODY()

	/** Tag query matched against each consumed ingredient's definition AssetTags plus instance tags (ArcCraftItemTags::GetCraftTags).
	 *  Every read slot whose ingredient satisfies the query adds one roll to the stat's summed result.
	 *  Empty query is intentionally inert — the row produces zero modifiers and is treated as
	 *  a designer-disabled entry, not a validation error. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry")
	FGameplayTagQuery IngredientQuery;

	/** Stat key to read from the ingredient's FArcItemFragment_ItemStatRanges
	 *  and write to the output's FArcItemFragment_ItemStats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry")
	FArcAttributeRef Stat;

	/** Band position bias per slot rarity step above 1 (BiasPerR of ArcCraftRarity::ComputeBandValue).
	 *  0 = no bias (uniform roll), 1 = standard linear bias, >1 = aggressive. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry", meta = (ClampMin = "0.0"))
	float RBias = 1.0f;

	/** Mod type applied to the output stat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry")
	EArcModType OutputModType = EArcModType::Additive;

	/** Weight of this part's roll in the summed result for the stat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry", meta = (ClampMin = "0.0"))
	float Share = 1.0f;

	/** Recipe slot this entry reads, still guarded by IngredientQuery; INDEX_NONE reads every slot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Entry", meta = (ClampMin = "-1"))
	int32 SlotIndex = INDEX_NONE;
};
