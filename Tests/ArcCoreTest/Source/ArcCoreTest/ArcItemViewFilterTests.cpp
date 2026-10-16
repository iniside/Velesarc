// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Items/ArcItemViewFilter.h"
#include "Items/ArcItemListEntry.h"
#include "GameplayTagsManager.h"

namespace ArcItemViewFilterTestHelpers
{
	static FGameplayTag TagA()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.Filter.A"));
	}

	static FGameplayTag TagB()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.Filter.B"));
	}

	static FGameplayTag TagC()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.Filter.C"));
	}

	static FGameplayTag TagX()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.Filter.X"));
	}

	static FGameplayTag TagY()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.Filter.Y"));
	}

	static FGameplayTag SlotHandRight()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.Filter.Slot.Hand.Right"));
	}

	static FGameplayTag SlotHandLeft()
	{
		return UGameplayTagsManager::Get().AddNativeGameplayTag(TEXT("Test.Filter.Slot.Hand.Left"));
	}
}

// ===================================================================
// FArcItemViewFilter::Matches tests
// ===================================================================

TEST_CLASS(ArcItemViewFilter_Matches, "ArcCore.UI.ItemViewFilter")
{
	TEST_METHOD(EmptyFilter_PassesEverything)
	{
		FArcItemViewFilter Filter;
		ASSERT_THAT(IsTrue(Filter.IsEmpty(), TEXT("Default-constructed filter must report IsEmpty()==true")));

		// Row with tags and a valid slot — empty filter should still pass.
		FArcItemListEntry Row;
		Row.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		Row.Tags.AddTag(ArcItemViewFilterTestHelpers::TagX());
		Row.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		Row.Level = 42;

		ASSERT_THAT(IsTrue(Filter.Matches(Row), TEXT("Empty filter should accept any row")));
	}

	TEST_METHOD(RequiredTags_HasAll_RequiresAllTags)
	{
		FArcItemViewFilter Filter;
		Filter.RequiredTags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		Filter.RequiredTags.AddTag(ArcItemViewFilterTestHelpers::TagB());

		// Only A — missing B → reject.
		FArcItemListEntry RowOnlyA;
		RowOnlyA.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		ASSERT_THAT(IsFalse(Filter.Matches(RowOnlyA), TEXT("Row with only A should be rejected (B missing)")));

		// A + B → pass.
		FArcItemListEntry RowAB;
		RowAB.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		RowAB.Tags.AddTag(ArcItemViewFilterTestHelpers::TagB());
		ASSERT_THAT(IsTrue(Filter.Matches(RowAB), TEXT("Row with A+B should be accepted")));

		// A + B + C → pass (superset of required).
		FArcItemListEntry RowABC;
		RowABC.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		RowABC.Tags.AddTag(ArcItemViewFilterTestHelpers::TagB());
		RowABC.Tags.AddTag(ArcItemViewFilterTestHelpers::TagC());
		ASSERT_THAT(IsTrue(Filter.Matches(RowABC), TEXT("Row with A+B+C (superset) should be accepted")));
	}

	TEST_METHOD(IgnoreTags_HasAny_RejectsAnyMatch)
	{
		FArcItemViewFilter Filter;
		Filter.IgnoreTags.AddTag(ArcItemViewFilterTestHelpers::TagX());

		// Row without X → pass.
		FArcItemListEntry RowNoX;
		RowNoX.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		ASSERT_THAT(IsTrue(Filter.Matches(RowNoX), TEXT("Row without X should be accepted")));

		// Row with X → reject.
		FArcItemListEntry RowX;
		RowX.Tags.AddTag(ArcItemViewFilterTestHelpers::TagX());
		ASSERT_THAT(IsFalse(Filter.Matches(RowX), TEXT("Row with X should be rejected")));

		// Row with X + Y → reject (X is still present).
		FArcItemListEntry RowXY;
		RowXY.Tags.AddTag(ArcItemViewFilterTestHelpers::TagX());
		RowXY.Tags.AddTag(ArcItemViewFilterTestHelpers::TagY());
		ASSERT_THAT(IsFalse(Filter.Matches(RowXY), TEXT("Row with X+Y should be rejected (X is an ignore tag)")));
	}

	TEST_METHOD(SlotPolicy_Any_AlwaysPasses)
	{
		FArcItemViewFilter Filter;
		Filter.SlotPolicy = EArcItemViewSlotPolicy::Any;

		// Unslotted row → pass.
		FArcItemListEntry RowNoSlot;
		ASSERT_THAT(IsTrue(Filter.Matches(RowNoSlot), TEXT("Any policy: unslotted row should pass")));

		// Slotted row → pass.
		FArcItemListEntry RowSlotted;
		RowSlotted.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		ASSERT_THAT(IsTrue(Filter.Matches(RowSlotted), TEXT("Any policy: slotted row should pass")));

		// Different slot → pass.
		FArcItemListEntry RowOtherSlot;
		RowOtherSlot.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandLeft();
		ASSERT_THAT(IsTrue(Filter.Matches(RowOtherSlot), TEXT("Any policy: any slot should pass")));
	}

	TEST_METHOD(SlotPolicy_UnslottedOnly_RejectsAnySlot)
	{
		FArcItemViewFilter Filter;
		Filter.SlotPolicy = EArcItemViewSlotPolicy::UnslottedOnly;

		// Invalid (no) slot → pass.
		FArcItemListEntry RowNoSlot;
		ASSERT_THAT(IsTrue(Filter.Matches(RowNoSlot), TEXT("UnslottedOnly: row with no slot should be accepted")));

		// Valid slot → reject.
		FArcItemListEntry RowSlotted;
		RowSlotted.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		ASSERT_THAT(IsFalse(Filter.Matches(RowSlotted), TEXT("UnslottedOnly: row with a valid slot should be rejected")));
	}

	TEST_METHOD(SlotPolicy_SlottedOnly_RejectsUnslotted)
	{
		FArcItemViewFilter Filter;
		Filter.SlotPolicy = EArcItemViewSlotPolicy::SlottedOnly;

		// Valid slot → pass.
		FArcItemListEntry RowSlotted;
		RowSlotted.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		ASSERT_THAT(IsTrue(Filter.Matches(RowSlotted), TEXT("SlottedOnly: row with a valid slot should be accepted")));

		// No slot → reject.
		FArcItemListEntry RowNoSlot;
		ASSERT_THAT(IsFalse(Filter.Matches(RowNoSlot), TEXT("SlottedOnly: row with no slot should be rejected")));
	}

	TEST_METHOD(SlotPolicy_MatchSlot_RequiresExactSlot)
	{
		FArcItemViewFilter Filter;
		Filter.SlotPolicy = EArcItemViewSlotPolicy::MatchSlot;
		Filter.MatchSlot = ArcItemViewFilterTestHelpers::SlotHandRight();

		// Exact match → pass.
		FArcItemListEntry RowMatch;
		RowMatch.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		ASSERT_THAT(IsTrue(Filter.Matches(RowMatch), TEXT("MatchSlot: matching slot should be accepted")));

		// Different slot → reject.
		FArcItemListEntry RowWrongSlot;
		RowWrongSlot.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandLeft();
		ASSERT_THAT(IsFalse(Filter.Matches(RowWrongSlot), TEXT("MatchSlot: different slot should be rejected")));

		// No slot (invalid) → reject.
		FArcItemListEntry RowNoSlot;
		ASSERT_THAT(IsFalse(Filter.Matches(RowNoSlot), TEXT("MatchSlot: unslotted row should be rejected")));
	}

	TEST_METHOD(LevelRange_RejectsBelowMin)
	{
		FArcItemViewFilter Filter;
		Filter.MinLevel = 5;

		FArcItemListEntry RowLevel4;
		RowLevel4.Level = 4;
		ASSERT_THAT(IsFalse(Filter.Matches(RowLevel4), TEXT("Level 4 is below MinLevel=5 and should be rejected")));

		FArcItemListEntry RowLevel5;
		RowLevel5.Level = 5;
		ASSERT_THAT(IsTrue(Filter.Matches(RowLevel5), TEXT("Level 5 == MinLevel=5 should be accepted")));
	}

	TEST_METHOD(LevelRange_RejectsAboveMax)
	{
		FArcItemViewFilter Filter;
		Filter.MaxLevel = 10;

		FArcItemListEntry RowLevel10;
		RowLevel10.Level = 10;
		ASSERT_THAT(IsTrue(Filter.Matches(RowLevel10), TEXT("Level 10 == MaxLevel=10 should be accepted")));

		FArcItemListEntry RowLevel11;
		RowLevel11.Level = 11;
		ASSERT_THAT(IsFalse(Filter.Matches(RowLevel11), TEXT("Level 11 is above MaxLevel=10 and should be rejected")));
	}

	TEST_METHOD(Combined_AllCriteriaApplied)
	{
		// Filter: RequiredTags={A}, IgnoreTags={X}, SlotPolicy=SlottedOnly, MinLevel=3, MaxLevel=7
		FArcItemViewFilter Filter;
		Filter.RequiredTags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		Filter.IgnoreTags.AddTag(ArcItemViewFilterTestHelpers::TagX());
		Filter.SlotPolicy = EArcItemViewSlotPolicy::SlottedOnly;
		Filter.MinLevel = 3;
		Filter.MaxLevel = 7;

		// Passing row: has A, no X, valid slot, level 5.
		FArcItemListEntry RowPass;
		RowPass.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		RowPass.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		RowPass.Level = 5;
		ASSERT_THAT(IsTrue(Filter.Matches(RowPass), TEXT("Combined: row satisfying all criteria should be accepted")));

		// Fail: missing required tag A.
		FArcItemListEntry RowNoA;
		RowNoA.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		RowNoA.Level = 5;
		ASSERT_THAT(IsFalse(Filter.Matches(RowNoA), TEXT("Combined: missing required tag A should be rejected")));

		// Fail: has ignored tag X.
		FArcItemListEntry RowHasX;
		RowHasX.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		RowHasX.Tags.AddTag(ArcItemViewFilterTestHelpers::TagX());
		RowHasX.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		RowHasX.Level = 5;
		ASSERT_THAT(IsFalse(Filter.Matches(RowHasX), TEXT("Combined: presence of ignored tag X should be rejected")));

		// Fail: not slotted.
		FArcItemListEntry RowUnslotted;
		RowUnslotted.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		RowUnslotted.Level = 5;
		ASSERT_THAT(IsFalse(Filter.Matches(RowUnslotted), TEXT("Combined: unslotted row should be rejected by SlottedOnly policy")));

		// Fail: level too low.
		FArcItemListEntry RowLowLevel;
		RowLowLevel.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		RowLowLevel.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		RowLowLevel.Level = 2;
		ASSERT_THAT(IsFalse(Filter.Matches(RowLowLevel), TEXT("Combined: level 2 below MinLevel=3 should be rejected")));

		// Fail: level too high.
		FArcItemListEntry RowHighLevel;
		RowHighLevel.Tags.AddTag(ArcItemViewFilterTestHelpers::TagA());
		RowHighLevel.CurrentItemStoreSlot = ArcItemViewFilterTestHelpers::SlotHandRight();
		RowHighLevel.Level = 8;
		ASSERT_THAT(IsFalse(Filter.Matches(RowHighLevel), TEXT("Combined: level 8 above MaxLevel=7 should be rejected")));
	}

	TEST_METHOD(IsEmpty_FalseWhenAnyFieldSet)
	{
		// RequiredTags
		{
			FArcItemViewFilter Filter;
			Filter.RequiredTags.AddTag(ArcItemViewFilterTestHelpers::TagA());
			ASSERT_THAT(IsFalse(Filter.IsEmpty(), TEXT("Non-empty RequiredTags should make IsEmpty false")));
		}
		// IgnoreTags
		{
			FArcItemViewFilter Filter;
			Filter.IgnoreTags.AddTag(ArcItemViewFilterTestHelpers::TagX());
			ASSERT_THAT(IsFalse(Filter.IsEmpty(), TEXT("Non-empty IgnoreTags should make IsEmpty false")));
		}
		// SlotPolicy
		{
			FArcItemViewFilter Filter;
			Filter.SlotPolicy = EArcItemViewSlotPolicy::SlottedOnly;
			ASSERT_THAT(IsFalse(Filter.IsEmpty(), TEXT("Non-default SlotPolicy should make IsEmpty false")));
		}
		// MinLevel
		{
			FArcItemViewFilter Filter;
			Filter.MinLevel = 1;
			ASSERT_THAT(IsFalse(Filter.IsEmpty(), TEXT("MinLevel=1 should make IsEmpty false")));
		}
		// MaxLevel
		{
			FArcItemViewFilter Filter;
			Filter.MaxLevel = 100;
			ASSERT_THAT(IsFalse(Filter.IsEmpty(), TEXT("MaxLevel=100 (non-MAX_int32) should make IsEmpty false")));
		}
	}
};
