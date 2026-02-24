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

/** Strategy for selecting which modifiers fill the available recipe-level slots. */
UENUM(BlueprintType)
enum class EArcCraftSlotSelection : uint8
{
	/** Fill slots with the modifiers that have the highest effective weight/quality. */
	HighestWeight,
	/** Fill slots by picking randomly among eligible modifiers. */
	Random
};

/**
 * Defines a single modifier slot at recipe level.
 *
 * Each entry in the recipe's ModifierSlots array represents one "chair" that
 * a single output modifier can fill. The SlotTag identifies the category — only
 * modifiers whose SlotTag matches can occupy this slot.
 *
 * Duplicate tags are allowed: adding two entries with SlotTag "Modifier.Offense"
 * means up to two offense modifiers can be assigned.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcCraftModifierSlot
{
	GENERATED_BODY()

public:
	/** Tag identifying this slot category (e.g. "Modifier.Offense"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FGameplayTag SlotTag;
};
