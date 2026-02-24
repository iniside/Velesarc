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

#include "ArcMaterialModifierSlotConfig.generated.h"

/** How to select among evaluations competing for the same modifier slot.
 *  DEPRECATED: Prefer recipe-level slot management via FArcCraftModifierSlot on UArcRecipeDefinition.
 *  This enum is only used by the legacy per-MaterialProperties slot filtering path
 *  (FArcMaterialCraftEvaluator::FilterBySlotConfig). */
UENUM(BlueprintType)
enum class EArcModifierSlotSelection : uint8
{
	/** Pick the N evaluations with the highest effective weight. */
	HighestWeight,
	/** Pick N evaluations at random (uniform). */
	Random,
	/** Allow all evaluations through (no limit enforcement for this slot). */
	All
};

/**
 * Defines a modifier slot that constrains how many evaluations of a given
 * type can apply to the output. Rules' OutputTags must contain the SlotTag
 * to be routed to this slot.
 *
 * DEPRECATED: Prefer recipe-level slot management via FArcCraftModifierSlot on UArcRecipeDefinition.
 * This struct is used by FArcRecipeOutputModifier_MaterialProperties::ModifierSlotConfigs for
 * backward compatibility with the legacy direct-apply path. When recipe-level ModifierSlots
 * are configured, this per-MaterialProperties slot config is ignored.
 */
USTRUCT(BlueprintType)
struct ARCCRAFT_API FArcMaterialModifierSlotConfig
{
	GENERATED_BODY()

public:
	/** Tag identifying this slot category (e.g., "Modifier.Offense"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	FGameplayTag SlotTag;

	/** Maximum number of evaluations that can contribute to this slot.
	 *  0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot", meta = (ClampMin = "0"))
	int32 MaxCount = 1;

	/** How to pick among competing evaluations when count exceeds MaxCount. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot")
	EArcModifierSlotSelection SelectionMode = EArcModifierSlotSelection::HighestWeight;
};
