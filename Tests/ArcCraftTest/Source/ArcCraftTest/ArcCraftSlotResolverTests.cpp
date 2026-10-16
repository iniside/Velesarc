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
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "Items/ArcItemSpec.h"
#include "Math/RandomStream.h"

// ---- Tags for slot resolver tests (unique prefix) ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Offense, "Test.Modifier.Offense");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Defense, "Test.Modifier.Defense");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Utility, "Test.Modifier.Utility");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Magic, "Test.Modifier.Magic");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Stealth, "Test.Modifier.Stealth");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Unknown, "Test.Modifier.Unknown");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Stat, "Test.Modifier.Stat");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_SlotTest_Active, "Test.Modifier.Active");

// ===================================================================
// Helpers
// ===================================================================

namespace SlotResolverTestHelpers
{
	const FRandomStream TestStream(12345);

	int32 NextGroupIndex = 0;

	/** Create a pending modifier with a slot tag and weight. */
	FArcCraftPendingModifier MakePending(
		FGameplayTag InSlotTag,
		float InWeight,
		int32 InModifierIndex = INDEX_NONE)
	{
		FArcCraftPendingModifier Pending;
		if (InSlotTag.IsValid())
		{
			Pending.SlotTags.AddTag(InSlotTag);
		}
		Pending.EffectiveWeight = InWeight;
		Pending.ModifierIndex = InModifierIndex;
		Pending.GroupIndex = NextGroupIndex++;
		return Pending;
	}

	/** Create a pending modifier with a stat result carrying a specific value.
	 *  InStatTag goes into StatTags so two results are different stats (the resolver dedupes equal stats). */
	FArcCraftPendingModifier MakePendingWithStat(
		FGameplayTag InSlotTag,
		float InWeight,
		float StatValue,
		FGameplayTag InStatTag)
	{
		FArcCraftPendingModifier Pending;
		if (InSlotTag.IsValid())
		{
			Pending.SlotTags.AddTag(InSlotTag);
		}
		Pending.EffectiveWeight = InWeight;
		Pending.GroupIndex = NextGroupIndex++;
		Pending.Result.Type = EArcCraftModifierResultType::Stat;
		Pending.Result.Stat.SetValue(StatValue);
		Pending.Result.Stat.StatTags.AddTag(InStatTag);
		return Pending;
	}

	/** Create an unslotted pending modifier (empty SlotTags). */
	FArcCraftPendingModifier MakeUnslotted(float InWeight, int32 InModifierIndex = INDEX_NONE)
	{
		FArcCraftPendingModifier Pending;
		// SlotTags intentionally empty = unslotted
		Pending.EffectiveWeight = InWeight;
		Pending.ModifierIndex = InModifierIndex;
		return Pending;
	}

	/** Create a single slot definition (one "chair" for the given tag). */
	FArcCraftModifierSlot MakeSlot(FGameplayTag InTag)
	{
		FArcCraftModifierSlot Slot;
		if (InTag.IsValid())
		{
			Slot.SlotTags.AddTag(InTag);
		}
		return Slot;
	}

	/** Create a flex slot definition (one "chair" offering two tags). */
	FArcCraftModifierSlot MakeFlexSlot(FGameplayTag InTagA, FGameplayTag InTagB)
	{
		FArcCraftModifierSlot Slot;
		if (InTagA.IsValid()) { Slot.SlotTags.AddTag(InTagA); }
		if (InTagB.IsValid()) { Slot.SlotTags.AddTag(InTagB); }
		return Slot;
	}

	/** Create a pending modifier requiring two slot tags. */
	FArcCraftPendingModifier MakePendingMultiTag(
		FGameplayTag InTagA,
		FGameplayTag InTagB,
		float InWeight)
	{
		FArcCraftPendingModifier Pending;
		if (InTagA.IsValid()) { Pending.SlotTags.AddTag(InTagA); }
		if (InTagB.IsValid()) { Pending.SlotTags.AddTag(InTagB); }
		Pending.EffectiveWeight = InWeight;
		Pending.GroupIndex = NextGroupIndex++;
		return Pending;
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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

		ASSERT_THAT(AreEqual(0, Result.Num(), TEXT("Empty input should return empty result")));
	}

	TEST_METHOD(AllUnslotted_NoSlots_AllPass)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(1.0f));
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(2.0f));
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(3.0f));

		TArray<FArcCraftModifierSlot> Slots;

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

		// Both unslotted (indices 0, 2) + the one slotted (index 1) should pass
		ASSERT_THAT(AreEqual(3, Result.Num(), TEXT("2 unslotted + 1 slotted = 3 total")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0), TEXT("Unslotted 0 should pass")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 1), TEXT("Slotted Offense should pass")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 2), TEXT("Unslotted 2 should pass")));
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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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
		TArray<int32> FirstResult = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);
		for (int32 Run = 0; Run < 10; ++Run)
		{
			TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);
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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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
// Result integration: verify that resolved modifiers carry correct results
// ===================================================================

TEST_CLASS(SlotResolver_Result, "ArcCraft.SlotResolver.Result")
{
	TEST_METHOD(ResolvedModifiers_ResultTypesPreserved)
	{
		TArray<FArcCraftPendingModifier> Pending;

		// Slotted stat with value 10 (weight=5, wins)
		Pending.Add(SlotResolverTestHelpers::MakePendingWithStat(TAG_SlotTest_Offense, 5.0f, 10.0f, TAG_SlotTest_Magic));
		// Slotted stat with value 100 (weight=1, loses)
		Pending.Add(SlotResolverTestHelpers::MakePendingWithStat(TAG_SlotTest_Offense, 1.0f, 100.0f, TAG_SlotTest_Stealth));
		// Unslotted stat with value 1000 (always passes)
		Pending.Add(SlotResolverTestHelpers::MakePendingWithStat(FGameplayTag(), 0.0f, 1000.0f, TAG_SlotTest_Utility));

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Offense)); // Only 1 Offense chair

		TArray<int32> ResolvedIndices = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

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

// ===================================================================
// D-21: FGameplayTagContainer slots — HasAll matching + narrowest chair
// ===================================================================

TEST_CLASS(SlotResolver_ContainerSlots, "ArcCraft.SlotResolver.ContainerSlots")
{
	// (a) flex chair {Stat,Active} accepts a Stat-only candidate and an Active-only candidate
	TEST_METHOD(FlexChair_AcceptsStatCandidate)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Stat, 5.0f)); // idx 0

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeFlexSlot(TAG_SlotTest_Stat, TAG_SlotTest_Active));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

		ASSERT_THAT(AreEqual(1, Result.Num(), TEXT("Flex chair {Stat,Active} should accept Stat-only candidate")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));
	}

	TEST_METHOD(FlexChair_AcceptsActiveCandidate)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Active, 5.0f)); // idx 0

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeFlexSlot(TAG_SlotTest_Stat, TAG_SlotTest_Active));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

		ASSERT_THAT(AreEqual(1, Result.Num(), TEXT("Flex chair {Stat,Active} should accept Active-only candidate")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));
	}

	// (b) candidate {Stat,Active} does NOT sit in chair {Stat} (HasAll fails), but DOES in chair {Stat,Active}
	TEST_METHOD(MultiTagCandidate_RequiresExactOrSuperset_Chair)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePendingMultiTag(TAG_SlotTest_Stat, TAG_SlotTest_Active, 5.0f)); // idx 0

		// Chair with only Stat: HasAll({Stat,Active}) on chair {Stat} — false
		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Stat));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

		ASSERT_THAT(AreEqual(0, Result.Num(), TEXT("Chair {Stat} must NOT accept candidate {Stat,Active}")));
	}

	TEST_METHOD(MultiTagCandidate_SitsInFlexChair)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePendingMultiTag(TAG_SlotTest_Stat, TAG_SlotTest_Active, 5.0f)); // idx 0

		// Chair {Stat,Active}: HasAll({Stat,Active}) — true
		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeFlexSlot(TAG_SlotTest_Stat, TAG_SlotTest_Active));

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

		ASSERT_THAT(AreEqual(1, Result.Num(), TEXT("Chair {Stat,Active} must accept candidate {Stat,Active}")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));
	}

	// (c) narrowest-chair heuristic: candidate {Stat} with chairs {Stat} and {Stat,Active} free — takes {Stat}
	TEST_METHOD(NarrowestChair_Heuristic)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Stat, 5.0f)); // idx 0: wants Stat

		TArray<FArcCraftModifierSlot> Slots;
		Slots.Add(SlotResolverTestHelpers::MakeFlexSlot(TAG_SlotTest_Stat, TAG_SlotTest_Active)); // chair 0: flex {Stat,Active} — 2 tags
		Slots.Add(SlotResolverTestHelpers::MakeSlot(TAG_SlotTest_Stat));                          // chair 1: narrow {Stat} — 1 tag

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

		ASSERT_THAT(AreEqual(1, Result.Num(), TEXT("Narrowest-chair: should assign to 1-tag chair, leaving flex free")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));

		// Verify the flex chair is still available (i.e., a second candidate can sit there)
		Pending.Add(SlotResolverTestHelpers::MakePending(TAG_SlotTest_Active, 3.0f)); // idx 1: wants Active

		TArray<int32> Result2 = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);
		ASSERT_THAT(AreEqual(2, Result2.Num(), TEXT("With narrowest-chair, both candidates should get a chair")));
	}

	// (d) empty SlotTags candidate = unslotted, always applied
	TEST_METHOD(EmptySlotTags_IsUnslotted)
	{
		TArray<FArcCraftPendingModifier> Pending;
		Pending.Add(SlotResolverTestHelpers::MakeUnslotted(5.0f)); // idx 0: no SlotTags = unslotted

		TArray<FArcCraftModifierSlot> Slots; // No chairs at all

		TArray<int32> Result = FArcCraftSlotResolver::Resolve(Pending, Slots, nullptr, SlotResolverTestHelpers::TestStream);

		ASSERT_THAT(AreEqual(1, Result.Num(), TEXT("Empty SlotTags = unslotted, always passes")));
		ASSERT_THAT(IsTrue(SlotResolverTestHelpers::ContainsIndex(Result, 0)));
	}
};
