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
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Templates/SubclassOf.h"
#include "ItemFragments/ArcItemFragment_GrantedMassAbilities.h"
#include "Modifiers/ArcDirectModifier.h"
#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"

struct FRandomStream;
struct FArcCraftSlotSelector;
class UArcEffectDefinition;

/** Type tag for which field in FArcCraftModifierResult is valid. */
enum class EArcCraftModifierResultType : uint8
{
	None,
	Stat,
	Ability,
	Effect,
	AttributeModifier
};

/**
 * Calculated output of a single modifier — exactly one of stat, ability, effect, or attribute modifier.
 * Transient, not serialized.
 */
struct FArcCraftModifierResult
{
	EArcCraftModifierResultType Type = EArcCraftModifierResultType::None;

	/** Valid when Type == Stat. Pre-scaled stat value ready to apply. */
	FArcItemAttributeStat Stat;

	/** Valid when Type == Ability. */
	FArcMassAbilityEntry Ability;

	/** Valid when Type == Effect. */
	TObjectPtr<UArcEffectDefinition> Effect;

	/** Valid when Type == AttributeModifier. Direct attribute modifiers (no backing GE). */
	TArray<FArcDirectModifier> AttributeModifiers;
};

/**
 * Intermediate result from evaluating an output modifier.
 * Each entry represents exactly one atomic modifier result (stat, ability, or effect).
 * Compound modifiers (Random, Pool, Material) produce multiple entries.
 * Used only transiently during FArcCraftOutputBuilder::Build — not serialized.
 */
struct FArcCraftPendingModifier
{
	/** Chair tags of the producing modifier. Empty = unslotted. */
	FGameplayTagContainer SlotTags;

	/** Weight of the producing group (rule weight; the source Weight for pool entries and static sets).
	 *  Read by the slot selectors. */
	float EffectiveWeight = 0.0f;

	/** Index of the originating source in Recipe.ModifierSources (-1 for sub-results). */
	int32 ModifierIndex = INDEX_NONE;

	/** Group within the source: rule or pool-entry index, 0 for a static set, INDEX_NONE when ungrouped (transfer).
	 *  (ModifierIndex, GroupIndex) names the group. */
	int32 GroupIndex = INDEX_NONE;

	/** The group's required ingredient tags that the consumed ingredients hold. */
	FGameplayTagContainer MatchedTags;

	/** Consumed amount of the slots holding any of the group's required tags over the total consumed amount;
	 *  0 when the group has no tag requirements. */
	float IngredientShare = 0.0f;

	/** The calculated result to apply to the output item. */
	FArcCraftModifierResult Result;
};

/**
 * Stateless utility that decides which pending modifiers apply, from the recipe's chairs and selector.
 *
 * A group is the pending results sharing (ModifierIndex, GroupIndex).
 *  - Ungrouped results (GroupIndex == INDEX_NONE, e.g. transfer) and the non-penalty results of a group without
 *    bonus chair tags always apply.
 *  - A group's bonus chair tags are the union of the SlotTags of its non-penalty slotted results. The selector
 *    orders the groups; each takes one whole bonus chair (a chair without Modifier.Penalty whose SlotTags HasAll
 *    the group's tags), the narrowest free one. A group sharing a bonus stat or effect with a placed group keeps
 *    only the higher absolute value: a higher incoming group replaces the placed one, otherwise it is dropped.
 *  - A placed group's unslotted results apply with it.
 *  - Penalty chairs (Modifier.Penalty) are filled the same way, by the same selector, from the Modifier.Penalty
 *    results of the placed groups; a penalty on a stat a placed bonus carries never lands, and one penalty stat
 *    lands at most once.
 *  - Without chairs no slotted result applies.
 *
 * A stat is identified by attribute and stat tags (item stat) or attribute and tag requirements (wearer modifier).
 */
class ARCCRAFT_API FArcCraftSlotResolver
{
public:
	/**
	 * @param PendingModifiers   All evaluated pending modifiers.
	 * @param Slots              Recipe's chairs.
	 * @param Selector           Candidate order; null = HighestWeight.
	 * @param Stream             The craft's stream, passed to the selector.
	 * @return                   Ascending indices into PendingModifiers that apply.
	 */
	static TArray<int32> Resolve(
		const TArray<FArcCraftPendingModifier>& PendingModifiers,
		const TArray<FArcCraftModifierSlot>& Slots,
		const FArcCraftSlotSelector* Selector,
		const FRandomStream& Stream);
};
