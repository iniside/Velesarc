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

#include "CQTest.h"

#include "NativeGameplayTags.h"
#include "ArcCraft/Recipe/ArcCraftModifierSlot.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "Items/ArcItemSpec.h"

// ---- Tags for slot resolver tests (unique prefix) ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Offense, "Modifier.Offense");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Defense, "Modifier.Defense");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Utility, "Modifier.Utility");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Magic, "Modifier.Magic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Stealth, "Modifier.Stealth");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Unknown, "Modifier.Unknown");

// ===================================================================
// Helpers
// ===================================================================

namespace SlotResolverTestHelpers
{
	/** Create a pending modifier with a slot tag and weight. */
	FArcCraftPendingModifier MakePending(
		FGameplayTag InSlotTag,
		float InWeight,
		int32 InModifierIndex = INDEX_NONE)
	{
		FArcCraftPendingModifier Pending;
		Pending.SlotTag = InSlotTag;
		Pending.EffectiveWeight = InWeight;
		Pending.ModifierIndex = InModifierIndex;
		return Pending;
	}

	/** Create a pending modifier with a stat result carrying a specific value. */
	FArcCraftPendingModifier MakePendingWithStat(
		FGameplayTag InSlotTag,
		float InWeight,
		float StatValue)
	{
		FArcCraftPendingModifier Pending;
		Pending.SlotTag = InSlotTag;
		Pending.EffectiveWeight = InWeight;
		Pending.Result.Type = EArcCraftModifierResultType::Stat;
		Pending.Result.Stat.SetValue(StatValue);
		return Pending;
	}

	/** Create an unslotted pending modifier (empty SlotTag). */
	FArcCraftPendingModifier MakeUnslotted(float InWeight, int32 InModifierIndex = INDEX_NONE)
	{
		return MakePending(FGameplayTag(), InWeight, InModifierIndex);
	}

	/** Create a single slot definition (one "chair" for the given tag). */
	FArcCraftModifierSlot MakeSlot(FGameplayTag InTag)
	{
		FArcCraftModifierSlot Slot;
		Slot.SlotTag = InTag;
		return Slot;
	}

	/** Check if a specific index is present in the result array. */
	bool ContainsIndex(const TArray<int32>& Indices, int32 Target)
	{
		return Indices.Contains(Target);
	}
}

// ===================================================================
// Empty / trivial cases
// ===================================================================

TEST_CLASS(SlotResolver_EmptyCases, "ArcCraft.SlotResolver.Empty")
{
	TEST_METHOD(EmptyPendingModifiers_ReturnsEmpty)
	{
		TArray<FArcCraftPendingModifier> Pending;
		TArray<FArcCraftModifierSlot> Slots;

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(0, Result.Num(), TEXT("Empty input should return empty result")));
	}

	TEST_METHOD(AllUnslotted_NoSlots_AllPass)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(1.0f));
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(2.0f));
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(3.0f));

		TArray<FArcCraftModifierSlot> Slots;

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(3, Result.Num(), TEXT("All unslotted modifiers should pass through")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1)));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 2)));
	}

	TEST_METHOD(AllSlotted_NoSlotDefinitions_AllDiscarded)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 3.0f));

		TArray<FArcCraftModifierSlot> Slots; // No slot definitions

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		// Slotted modifiers with no matching slot definition should be discarded
		ASSERT_THAT(AreEqual(0, Result.Num(), TEXT("Slotted modifiers with no matching slot definitions should be discarded")));
	}
};

// ===================================================================
// Unslotted modifiers always pass through
// ===================================================================

TEST_CLASS(SlotResolver_Unslotted, "ArcCraft.SlotResolver.Unslotted")
{
	TEST_METHOD(UnslottedBypassSlotResolution)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(1.0f));
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(2.0f));

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		// Both unslotted (indices 0, 2) + the one slotted (index 1) should pass
		ASSERT_THAT(AreEqual(3, Result.Num(), TEXT("2 unslotted + 1 slotted = 3 total")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0), TEXT("Unslotted 0 should pass")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1), TEXT("Slotted Offense should pass")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 2), TEXT("Unslotted 2 should pass")));
	}

	TEST_METHOD(UnslottedNotAffectedByMaxUsableSlots)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(1.0f));
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(2.0f));
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(3.0f));
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 10.0f));

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));

		// MaxUsableSlots = 1 — only limits slotted modifiers
		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 1, EArcCraftSlotSelection::HighestWeight);

		// 3 unslotted + 1 slotted (within cap)
		ASSERT_THAT(AreEqual(4, Result.Num(), TEXT("3 unslotted + 1 slotted within cap = 4")));
	}
};

// ===================================================================
// Single slot per tag — one "chair" per entry
// ===================================================================

TEST_CLASS(SlotResolver_SingleSlot, "ArcCraft.SlotResolver.SingleSlot")
{
	TEST_METHOD(OneSlot_OneModifier_Assigned)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(1, Result.Num(), TEXT("1 slot, 1 matching modifier = 1 assigned")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));
	}

	TEST_METHOD(OneSlot_MultipleModifiers_HighestWeightWins)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 3.0f));  // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 7.0f));  // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));  // idx 2

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // 1 chair

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(1, Result.Num(), TEXT("1 chair for Offense should keep exactly 1")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1),
			TEXT("Should keep index 1 (weight=7.0, highest)")));
	}

	TEST_METHOD(MultipleSlotsDifferentTags_OneModifierEach)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));  // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 3.0f));  // idx 1

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("2 slots, 2 matching modifiers = 2 assigned")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1)));
	}

	TEST_METHOD(SlottedModifier_NoMatchingSlotDef_Discarded)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Unknown, 10.0f)); // Unknown slot

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // Only Offense defined

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(0, Result.Num(),
			TEXT("Modifier targeting undefined slot should be discarded")));
	}

	TEST_METHOD(EqualWeights_DeterministicTiebreak)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));  // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));  // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));  // idx 2

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // 1 chair

		// Run multiple times — result must always be the same
		TArray<int32> FirstResult = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);
		for (int32 Run = 0; Run < 10; ++Run)
		{
			TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);
			ASSERT_THAT(AreEqual(1, Result.Num()));
			ASSERT_THAT(AreEqual(FirstResult[0], Result[0],
				TEXT("Equal-weight tiebreak must be deterministic across runs")));
		}

		// With deterministic tiebreak (lower index wins), index 0 should be kept
		ASSERT_THAT(AreEqual(0, FirstResult[0],
			TEXT("Equal weights: lower index should win (deterministic tiebreak)")));
	}
};

// ===================================================================
// Duplicate tags — multiple "chairs" for the same category
// ===================================================================

TEST_CLASS(SlotResolver_DuplicateSlots, "ArcCraft.SlotResolver.DuplicateSlots")
{
	TEST_METHOD(TwoOffenseSlots_TwoModifiers_BothAssigned)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));  // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 3.0f));  // idx 1

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // chair 1
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // chair 2

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("2 Offense chairs, 2 Offense modifiers = 2 assigned")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1)));
	}

	TEST_METHOD(TwoOffenseSlots_ThreeModifiers_HighestTwoWin)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 1.0f));  // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 9.0f));  // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));  // idx 2

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // chair 1
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // chair 2

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("2 Offense chairs, 3 modifiers -> top 2 by weight")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1),
			TEXT("Index 1 (weight=9) should be assigned")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 2),
			TEXT("Index 2 (weight=5) should be assigned")));
		ASSERT_THAT(IsFalse(SlotResolverTestHelpers::ContainsIndex(Result, 0),
			TEXT("Index 0 (weight=1) should be cut")));
	}

	TEST_METHOD(MixedDuplicateSlots_CapacityPerTag)
	{
		TArray<FArcCraftPendingModifier> Pending;
		// 3 Offense modifiers
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 8.0f));  // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 6.0f));  // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 4.0f));  // idx 2
		// 2 Defense modifiers
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 7.0f));  // idx 3
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 3.0f));  // idx 4

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // 2 Offense chairs
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense)); // 1 Defense chair

		// 3 total chairs, no MaxUsableSlots cap
		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(3, Result.Num(), TEXT("2 Offense + 1 Defense = 3 chairs filled")));
		// Offense: top 2 = idx 0 (8.0), idx 1 (6.0)
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0),
			TEXT("Offense idx 0 (weight=8) assigned")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1),
			TEXT("Offense idx 1 (weight=6) assigned")));
		// Defense: top 1 = idx 3 (7.0)
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 3),
			TEXT("Defense idx 3 (weight=7) assigned")));
	}
};

// ===================================================================
// MaxUsableSlots — hard cap on total filled slots
// ===================================================================

TEST_CLASS(SlotResolver_MaxUsableSlots, "ArcCraft.SlotResolver.MaxUsableSlots")
{
	TEST_METHOD(MaxUsableSlots_Zero_NoCap)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 1.0f));
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 2.0f));
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Utility, 3.0f));

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Utility));

		// MaxUsableSlots=0 means no cap
		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(3, Result.Num(), TEXT("No cap should keep all 3")));
	}

	TEST_METHOD(MaxUsableSlots_LessThanSlots_HighestWeight)
	{
		// 5 slots defined, MaxUsableSlots=3, 5 modifiers (one per slot)
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 10.0f)); // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 1.0f));  // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Utility, 5.0f));  // idx 2
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Magic, 8.0f));    // idx 3
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Stealth, 3.0f));  // idx 4

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Utility));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Magic));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Stealth));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 3, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(3, Result.Num(), TEXT("5 slots, cap=3 should keep exactly 3")));
		// Top 3 by weight: Offense(10), Magic(8), Utility(5)
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0),
			TEXT("Offense (weight=10) should survive")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 3),
			TEXT("Magic (weight=8) should survive")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 2),
			TEXT("Utility (weight=5) should survive")));
		ASSERT_THAT(IsFalse(SlotResolverTestHelpers::ContainsIndex(Result, 1),
			TEXT("Defense (weight=1) should be cut")));
		ASSERT_THAT(IsFalse(SlotResolverTestHelpers::ContainsIndex(Result, 4),
			TEXT("Stealth (weight=3) should be cut")));
	}

	TEST_METHOD(MaxUsableSlots_MoreModifiersThanSlots_CapIsSlotCount)
	{
		// 8 modifiers, 5 slots, MaxUsableSlots=3
		// Only 5 can ever match (1 per slot tag), then capped to 3
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 10.0f)); // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 2.0f));  // idx 1 (competes for same slot)
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 8.0f));  // idx 2
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 1.0f));  // idx 3 (competes for same slot)
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Utility, 5.0f));  // idx 4
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Magic, 7.0f));    // idx 5
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Stealth, 3.0f));  // idx 6
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Unknown, 99.0f)); // idx 7 — no slot

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));  // 1 chair
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));  // 1 chair
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Utility));  // 1 chair
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Magic));    // 1 chair
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Stealth));  // 1 chair

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 3, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(3, Result.Num(), TEXT("8 modifiers, 5 slots, cap=3 -> 3 assigned")));
		// Top 3 by weight: Offense(10), Defense(8), Magic(7)
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0),
			TEXT("Offense idx 0 (weight=10) should survive")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 2),
			TEXT("Defense idx 2 (weight=8) should survive")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 5),
			TEXT("Magic idx 5 (weight=7) should survive")));
	}

	TEST_METHOD(MaxUsableSlots_WithDuplicateSlots_RespectsCapacity)
	{
		// 2× Offense slots, 1× Defense slot. MaxUsableSlots=2.
		// 3 Offense modifiers compete for 2 chairs, but cap=2 limits total.
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 9.0f));  // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 6.0f));  // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 3.0f));  // idx 2
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 7.0f));  // idx 3

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // chair 1
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // chair 2
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense)); // chair 3

		// 3 total chairs, but cap=2
		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 2, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("3 chairs, cap=2 -> 2 assigned")));
		// By HighestWeight: idx 0 (9.0), idx 3 (7.0)
		// idx 0 fills Offense chair, idx 3 fills Defense chair
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0),
			TEXT("Offense idx 0 (weight=9) should be assigned")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 3),
			TEXT("Defense idx 3 (weight=7) should be assigned")));
	}

	TEST_METHOD(MaxUsableSlots_Random_KeepsCorrectCount)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 1.0f));
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 2.0f));
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Utility, 3.0f));

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Utility));

		// Run multiple times with Random selection — count must always be 2
		for (int32 Run = 0; Run < 50; ++Run)
		{
			TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 2, EArcCraftSlotSelection::Random);
			ASSERT_THAT(AreEqual(2, Result.Num(),
				TEXT("Random selection with cap=2 should always keep exactly 2")));
		}
	}

	TEST_METHOD(MaxUsableSlots_Random_AllCandidatesCanAppear)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 1.0f));   // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 1.0f));   // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Utility, 1.0f));   // idx 2

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Utility));

		TSet<int32> ObservedIndices;
		for (int32 Run = 0; Run < 200; ++Run)
		{
			TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 2, EArcCraftSlotSelection::Random);
			for (int32 Idx : Result)
			{
				ObservedIndices.Add(Idx);
			}
		}

		// Over 200 runs, all 3 indices should appear at least once
		ASSERT_THAT(IsTrue(ObservedIndices.Contains(0), TEXT("Offense should appear in some runs")));
		ASSERT_THAT(IsTrue(ObservedIndices.Contains(1), TEXT("Defense should appear in some runs")));
		ASSERT_THAT(IsTrue(ObservedIndices.Contains(2), TEXT("Utility should appear in some runs")));
	}

	TEST_METHOD(MaxUsableSlots_GreaterThanSlotCount_CappedAtSlotCount)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 5.0f));
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 3.0f));

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));

		// MaxUsableSlots=10 > 2 defined slots — effective cap is 2
		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 10, EArcCraftSlotSelection::HighestWeight);

		ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("Cap=10 but only 2 slots, so 2 assigned")));
	}
};

// ===================================================================
// Mixed scenarios: unslotted + slotted + MaxUsableSlots
// ===================================================================

TEST_CLASS(SlotResolver_Mixed, "ArcCraft.SlotResolver.Mixed")
{
	TEST_METHOD(UnslottedPlusSlotted_CapOnlyAffectsSlotted)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(1.0f));                          // idx 0: unslotted
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 10.0f));     // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 5.0f));      // idx 2
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Utility, 2.0f));      // idx 3
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(99.0f));                         // idx 4: unslotted

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Utility));

		// MaxUsableSlots=2 — only 2 slotted can survive, but unslotted always pass
		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 2, EArcCraftSlotSelection::HighestWeight);

		// 2 unslotted + 2 slotted (highest weight) = 4 total
		ASSERT_THAT(AreEqual(4, Result.Num(), TEXT("2 unslotted + 2 slotted = 4")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0), TEXT("Unslotted 0 should pass")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 4), TEXT("Unslotted 4 should pass")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1), TEXT("Offense (weight=10) should survive")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 2), TEXT("Defense (weight=5) should survive")));
		ASSERT_THAT(IsFalse(SlotResolverTestHelpers::ContainsIndex(Result, 3), TEXT("Utility (weight=2) should be cut")));
	}

	TEST_METHOD(FullPipeline_8Modifiers_5Slots_3MaxUsable)
	{
		// The user's exact example: 8 modifiers, 5 slots, 3 max usable
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 10.0f));  // idx 0
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 4.0f));   // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 8.0f));   // idx 2
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 2.0f));   // idx 3
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Utility, 6.0f));   // idx 4
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Magic, 9.0f));     // idx 5
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Stealth, 3.0f));   // idx 6
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Unknown, 99.0f));  // idx 7 — no slot

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Utility));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Magic));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Stealth));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 3, EArcCraftSlotSelection::HighestWeight);

		// 8 evaluated → 7 eligible (Unknown discarded) → each slot has 1 chair
		// Multiple modifiers compete for Offense and Defense, only 1 per tag can fill
		// Top 3 by weight: Offense(10), Magic(9), Defense(8)
		ASSERT_THAT(AreEqual(3, Result.Num(), TEXT("8 mods, 5 slots, cap=3 -> 3 assigned")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0),
			TEXT("Offense idx 0 (weight=10) should survive")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 5),
			TEXT("Magic idx 5 (weight=9) should survive")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 2),
			TEXT("Defense idx 2 (weight=8) should survive")));
	}

	TEST_METHOD(FullPipeline_WithUnslottedAndUnknown)
	{
		TArray<FArcCraftPendingModifier> Pending;
		// Unslotted
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(1.0f));                          // idx 0: always passes
		// Slotted
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 8.0f));      // idx 1
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Offense, 3.0f));      // idx 2
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Defense, 6.0f));      // idx 3
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Unknown, 100.0f));    // idx 4: discarded

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense));
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Defense));

		// 2 slot chairs, cap=1 — only 1 slotted survives
		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, 1, EArcCraftSlotSelection::HighestWeight);

		// 1 unslotted + 1 slotted
		ASSERT_THAT(AreEqual(2, Result.Num(), TEXT("1 unslotted + 1 slotted")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0), TEXT("Unslotted passes")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1),
			TEXT("Offense (weight=8) beats Defense (weight=6)")));
		ASSERT_THAT(IsFalse(SlotResolverTestHelpers::ContainsIndex(Result, 4),
			TEXT("Unknown slot modifier should be discarded")));
	}
};

// ===================================================================
// Result integration: verify that resolved modifiers carry correct results
// ===================================================================

TEST_CLASS(SlotResolver_Result, "ArcCraft.SlotResolver.Result")
{
	TEST_METHOD(ResolvedModifiers_ResultTypesPreserved)
	{
		TArray<FArcCraftPendingModifier> Pending;

		// Slotted stat with value 10 (weight=5, wins)
		Pending.Add(SlotResolverTestHelpers::MakePendingWithStat(TAG_SlotTest_Offense, 5.0f, 10.0f));
		// Slotted stat with value 100 (weight=1, loses)
		Pending.Add(SlotResolverTestHelpers::MakePendingWithStat(TAG_SlotTest_Offense, 1.0f, 100.0f));
		// Unslotted stat with value 1000 (always passes)
		Pending.Add(SlotResolverTestHelpers::MakePendingWithStat(FGameplayTag(), 0.0f, 1000.0f));

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // Only 1 Offense chair

		TArray<int32> ResolvedIndices = FArcCraftSlotResolver::Resolve(Pending, Slots, 0, EArcCraftSlotSelection::HighestWeight);

		// Verify resolved modifier results
		float TotalStatValue = 0.0f;
		for (int32 Idx : ResolvedIndices)
		{
			if (Pending[Idx].Result.Type == EArcCraftModifierResultType::Stat)
			{
				TotalStatValue += Pending[Idx].Result.Stat.Value.GetValue();
			}
		}

		// Idx 0 (weight=5, stat=10) + Idx 2 (unslotted, stat=1000) should survive
		// Idx 1 (weight=1, stat=100) should NOT survive (cut — only 1 chair)
		ASSERT_THAT(IsNear(1010.0f, TotalStatValue, 0.001f,
			TEXT("Only resolved modifiers should contribute: 10 (offense winner) + 1000 (unslotted)")));
	}
};
