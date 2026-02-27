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
#include "Items/ArcItemTypes.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Templates/SubclassOf.h"
#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"

class UGameplayEffect;

/** Type tag for which field in FArcCraftModifierResult is valid. */
enum class EArcCraftModifierResultType : uint8
{
	None,
	Stat,
	Ability,
	Effect
};

/**
 * Calculated output of a single modifier — exactly one of stat, ability, or effect.
 * Transient, not serialized.
 */
struct FArcCraftModifierResult
{
	EArcCraftModifierResultType Type = EArcCraftModifierResultType::None;

	/** Valid when Type == Stat. Pre-scaled stat value ready to apply. */
	FArcItemAttributeStat Stat;

	/** Valid when Type == Ability. */
	FArcAbilityEntry Ability;

	/** Valid when Type == Effect. */
	TSubclassOf<UGameplayEffect> Effect;
};

/**
 * Intermediate result from evaluating an output modifier.
 * Each entry represents exactly one atomic modifier result (stat, ability, or effect).
 * Compound modifiers (Random, Pool, Material) produce multiple entries.
 * Used only transiently during BuildOutputSpec — not serialized.
 */
struct FArcCraftPendingModifier
{
	/** Which slot tag this modifier wants. Empty = unslotted (always applies). */
	FGameplayTag SlotTag;

	/** Weight/quality score used by HighestWeight selection. Higher = more desirable. */
	float EffectiveWeight = 0.0f;

	/** Index of the original modifier in Recipe->OutputModifiers (-1 for sub-results). */
	int32 ModifierIndex = INDEX_NONE;

	/** The calculated result to apply to the output item. */
	FArcCraftModifierResult Result;
};

/**
 * Stateless utility for resolving pending modifiers through recipe-level slot configuration.
 *
 * The resolver implements a two-category pipeline:
 *
 *  Unslotted modifiers (empty SlotTag) always apply — they bypass slots entirely.
 *
 *  Slotted modifiers compete for a limited number of slot "chairs":
 *   1. Each slot entry in ModifierSlots is one chair with a tag category.
 *      Duplicate tags are allowed (2× Offense = 2 chairs for offense modifiers).
 *   2. MaxUsableSlots limits how many chairs are actually available.
 *      If MaxUsableSlots < total slots, only that many get filled.
 *   3. SelectionMode decides which modifiers win when there are more
 *      candidates than available chairs (HighestWeight or Random).
 *   4. A modifier whose SlotTag doesn't match any slot definition is discarded.
 *
 * Returns indices into the PendingModifiers array that should be applied.
 */
class ARCCRAFT_API FArcCraftSlotResolver
{
public:
	/**
	 * Resolve pending modifiers through recipe-level slot configuration.
	 *
	 * @param PendingModifiers   All evaluated pending modifiers.
	 * @param Slots              Recipe's modifier slot definitions (each entry = one chair).
	 * @param MaxUsableSlots     Hard cap on how many slots get filled (0 = all slots usable).
	 * @param SelectionMode      How to pick which modifiers fill available slots.
	 * @return                   Indices into PendingModifiers that should be applied.
	 */
	static TArray<int32> Resolve(
		const TArray<FArcCraftPendingModifier>& PendingModifiers,
		const TArray<FArcCraftModifierSlot>& Slots,
		int32 MaxUsableSlots,
		EArcCraftSlotSelection SelectionMode);
};
