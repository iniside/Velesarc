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
#include "GameplayTagContainer.h"

#include "ArcCraftModifierSlot.generated.h"

/**
 * Defines a single modifier slot at recipe level.
 *
 * Each entry in the recipe's ModifierSlots array represents one "chair" that
 * a single output modifier can fill. SlotTags is the set of tags this chair
 * OFFERS. A candidate modifier fits when Chair.SlotTags.HasAll(Candidate.SlotTags).
 *
 * A chair with multiple tags, e.g. {Modifier.Stat, Modifier.Passive}, is a
 * "flex chair" — it accepts both Stat-only and Passive-only candidates, while
 * also accepting candidates that require both tags simultaneously.
 *
 * A chair holding Modifier.Penalty is a penalty chair; every other chair is a bonus chair.
 * Chairs with an empty SlotTags container are ignored.
 */
USTRUCT(BlueprintType, meta = (Tooltip = "Defines a single modifier slot (chair) at recipe level; a candidate modifier fits when Chair.SlotTags.HasAll(Candidate.SlotTags)."))
struct ARCCRAFT_API FArcCraftModifierSlot
{
	GENERATED_BODY()

public:
	/** Tags this chair offers. A candidate fits when Chair.SlotTags.HasAll(Candidate.SlotTags).
	 *  Modifier.Penalty makes it a penalty chair. Empty container = this chair is ignored. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FGameplayTagContainer SlotTags;
};
