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

/**
 * One ingredient slot in the multi-slot debug simulator.
 * Plain C++ struct — not UHT-reflected; the serialized mirror is
 * FArcDebugIngredientSlotSerialized in ArcCraftEditorDebugSettings.h.
 */
struct FArcDebugIngredientSlot
{
	FGameplayTagContainer Tags;
	float RarityStep = 1.0f;
};

/**
 * Editor-local debug input context for modifier preview.
 * Drives row tinting, weight chips and band previews in the tree view.
 */
struct FArcModifierDebugContext
{
	/** Average rarity step R for preview (default 1).
	 *  Used as fallback when InputSlots is empty. */
	float AverageR = 1.0f;

	/** Ingredient tags to test modifier TriggerTags against. */
	FGameplayTagContainer IngredientTags;

	/** Slot tags (used by MaterialPropertyTable rule matching). */
	FGameplayTagContainer SlotTags;

	/** Output rarity whose band the stat previews and conversions roll in; empty = full catalog range. */
	FGameplayTag OutputRarity;

	/** Crafter skill above the recipe's required level, as in FArcCraftContext::SkillMargin. */
	float SkillMargin = 0.0f;

	/** Whether the live-preview overlay is active. */
	bool bEnabled = false;

	/** Multi-slot ingredient simulator. When non-empty, Step 6 eval will use
	 *  these slots instead of the legacy R/IngredientTags fields. */
	TArray<FArcDebugIngredientSlot> InputSlots;
};

/**
 * Evaluation result for a single modifier row during debug preview.
 */
struct FArcModifierRowEvaluation
{
	/** Whether the row's tag/R rule was satisfied. */
	bool bRulePassed = false;

	/** Whether this modifier's own threshold was cleared by the current R. */
	bool bModifierEligible = false;

	/** Rule weight (ComputeRuleWeight) on rule rows; pool entry selection weight on entry rows. */
	float EffectiveWeight = 0.0f;

	/** Required tags of a rule or pool entry row that the debug ingredients hold. */
	FGameplayTagContainer MatchedTags;

	/** Consumed amount share of the slots holding the row's required tags (ArcCraftProvenance::ComputeIngredientShare). */
	float IngredientShare = 0.0f;

	/** True when a stat modifier resolved in the stat catalog; BandAtU0 and BandAtU1 are then valid. */
	bool bHasBand = false;

	/** Band edges for the debug rarity: ArcCraftRarity::ComputeBandValue at U = 0 and U = 1. */
	float BandAtU0 = 0.0f;
	float BandAtU1 = 0.0f;

	/** Human-readable reason if bRulePassed or bModifierEligible is false. */
	FText FailureReason;

	/**
	 * Human-readable output lines produced by the runtime conversion (ArcCraftModifiers::ConvertModifier).
	 * Populated only when bModifierEligible is true. Empty for ineligible rows.
	 *
	 * Examples:
	 *   "Stat: Damage.Physical +12.5 (Additive)"
	 *   "Ability: DA_SwordSlash"
	 *   "Effect: E_BurnDoT"
	 *   "Attribute: Health.Max +25.0 (Add)"
	 */
	TArray<FText> ResolvedOutputs;
};

/**
 * Full debug evaluation for one asset, keyed by a row key integer
 * (ChildIndex for flat lists, or encoded parent+child for nested).
 */
struct FArcModifierDebugEvaluation
{
	/** Per-row results. Key is (ParentIndex * 10000 + ChildIndex) for nested,
	 *  or just ChildIndex for flat modifier lists. */
	TMap<int32, FArcModifierRowEvaluation> ByRowKey;
};
