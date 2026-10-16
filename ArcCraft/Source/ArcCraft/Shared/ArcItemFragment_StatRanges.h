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
#include "Items/Fragments/ArcItemFragment.h"
#include "Attributes/ArcAttribute.h"

#include "ArcItemFragment_StatRanges.generated.h"

/**
 * Design-time range entry: the acceptable [Min, Max] interval for a single stat on a crafted item.
 * Used by the durability/stat transfer pipeline to validate or clamp ingredient stat contributions.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Design-time range entry defining the acceptable [Min, Max] interval for a single stat on a crafted item."))
struct ARCCRAFT_API FArcItemAttributeStatRange
{
	GENERATED_BODY()

	/** The attribute this range applies to. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Data")
	FArcAttributeRef Stat;

	/** Minimum acceptable value for this stat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Data")
	float Min = 0.0f;

	/** Maximum acceptable value for this stat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Data")
	float Max = 0.0f;

	/** Returns true when Stat is valid and the range is well-formed (Max >= Min). */
	bool IsValid() const { return Stat.IsValid() && Max >= Min; }
};

/**
 * Definition-time fragment: declares the stat ranges an item supports.
 * Placed on an item definition to constrain what stat values are meaningful for that item type.
 * The crafting pipeline reads these ranges when computing rarity-scaled outputs and when
 * transferring stats from ingredients to the output item.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Item Stat Ranges", Category = "Stats",
	ToolTip = "Declares the [Min, Max] range for each stat this item type supports. Used by the crafting pipeline for rarity-scaled stat transfer and validation."))
struct ARCCRAFT_API FArcItemFragment_ItemStatRanges : public FArcItemFragment
{
	GENERATED_BODY()

public:
	/** The set of stat ranges for this item type. One entry per supported attribute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Data",
		meta = (TitleProperty = "Stat", Tooltip = "The set of stat ranges for this item type; one entry per supported attribute."))
	TArray<FArcItemAttributeStatRange> Ranges;

	virtual ~FArcItemFragment_ItemStatRanges() override = default;

	/**
	 * Returns the first range entry whose Stat matches InStat, or nullptr if none found.
	 * Equality uses FArcAttributeRef::operator== (matches FragmentType + PropertyName).
	 */
	const FArcItemAttributeStatRange* FindRange(const FArcAttributeRef& InStat) const;
};
