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
#include "ArcItemTestRecordFiles.h"

#include "NativeGameplayTags.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Attributes/ArcAttribute.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "ArcCraft/Shared/ArcItemFragment_StatRanges.h"
#include "ArcCraft/StatTransfer/ArcStatTransferTableData.h"
#include "ArcCraft/StatTransfer/ArcStatTransferEntry.h"

#include "ArcCraftTestRecordFiles.h"

// ---- File-unique gameplay tag prefix: TAG_StatTransferTest_ ----
// Each tag represents a distinct ingredient identity matched via
// FArcStatTransferEntry::IngredientQuery against the ingredient's
// FArcItemFragment_Tags::AssetTags at Evaluate time.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatTransferTest_Ingredient_Wood,  "Test.StatTransfer.Ingredient.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatTransferTest_Ingredient_Stone, "Test.StatTransfer.Ingredient.Stone");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatTransferTest_Ingredient_Iron,  "Test.StatTransfer.Ingredient.Iron");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_StatTransferTest_Ingredient_None,  "Test.StatTransfer.Ingredient.None");

// ---------------------------------------------------------------------------
// Helper utilities (file-scoped namespace).
//
// FArcAttributeRef equality (and FindRange lookup) uses FragmentType +
// PropertyName — no actual fragment memory access occurs during Evaluate.
// We reuse FArcItemAttributeStatRange (already in scope via
// ArcItemFragment_StatRanges.h) and point at its "Min" float property.
// This avoids a new USTRUCT while still exercising the real FindRange path.
// ---------------------------------------------------------------------------
namespace ArcStatTransferTableTestsImpl
{

/**
 * Returns an FArcAttributeRef that points at the "Min" property on
 * FArcItemAttributeStatRange.  Stable anchor: FindRange compares
 * (FragmentType, PropertyName), and MakeSpecWithRange seeds a matching entry.
 */
static FArcAttributeRef MakeTestStatRef()
{
	FArcAttributeRef Ref;
	Ref.FragmentType = FArcItemAttributeStatRange::StaticStruct();
	Ref.PropertyName = GET_MEMBER_NAME_CHECKED(FArcItemAttributeStatRange, Min);
	return Ref;
}

/**
 * Write an item definition record with a FArcItemFragment_ItemStatRanges
 * containing one entry (TestStat, RangeMin, RangeMax) and optionally a
 * FArcItemFragment_Tags fragment whose AssetTags are matched by entry queries.
 * Returns an FArcItemSpec for the definition.
 */
static FArcItemSpec MakeSpecWithRange(FArcItemTestRecordFiles& ItemRecords, float RangeMin, float RangeMax,
                                      const FGameplayTagContainer& AssetTags = FGameplayTagContainer())
{
	FArcItemDefinitionData Def;

	FArcItemAttributeStatRange RangeEntry;
	RangeEntry.Stat = MakeTestStatRef();
	RangeEntry.Min  = RangeMin;
	RangeEntry.Max  = RangeMax;

	FArcItemFragment_ItemStatRanges RangesFragment;
	RangesFragment.Ranges.Add(RangeEntry);
	ArcItemTestRecords::AddFragment(Def, RangesFragment);

	if (!AssetTags.IsEmpty())
	{
		FArcItemFragment_Tags TagsFragment;
		TagsFragment.AssetTags = AssetTags;
		ArcItemTestRecords::AddFragment(Def, TagsFragment);
	}

	FArcItemSpec Spec;
	Spec.SetItemDefinition(ItemRecords.Write(Def));
	Spec.Amount = 1;
	return Spec;
}

/**
 * Build a single-entry stat transfer table record with the given parameters, write it
 * through Records and point a fresh source at its id.
 * The entry's IngredientQuery is built from RequiredTags (MatchAllTags); an empty
 * RequiredTags produces a default (inert) query that emits zero modifiers.
 */
static FArcRecipeModifierSource_StatTransferTable MakeSourceWithSingleEntry(
	FArcCraftTestRecordFiles& Records,
	const FGameplayTagContainer& RequiredTags,
	float RBias,
	EArcModType ModType)
{
	FArcStatTransferEntry Entry;
	Entry.IngredientQuery = RequiredTags.IsEmpty()
		? FGameplayTagQuery()
		: FGameplayTagQuery::MakeQuery_MatchAllTags(RequiredTags);
	Entry.Stat                = MakeTestStatRef();
	Entry.RBias         = RBias;
	Entry.OutputModType       = ModType;

	FArcStatTransferTableData Table;
	Table.Entries.Add(Entry);

	FArcRecipeModifierSource_StatTransferTable Source;
	Source.Table = Records.Write(Table);
	return Source;
}

/**
 * A context whose stream is seeded, so the rolls are reproducible. Callers keep one context across
 * their Evaluate calls: a fresh context would restart the stream and repeat the same draw.
 */
static FArcCraftContext MakeSeededContext(const TArray<float>& SlotRaritySteps, float AverageR, int32 Seed)
{
	FArcCraftContext Context;
	Context.SlotRaritySteps = SlotRaritySteps;
	Context.AverageR = AverageR;
	Context.Stream.Initialize(Seed);
	return Context;
}

} // namespace ArcStatTransferTableTestsImpl

// ===========================================================================
// Tests
// ===========================================================================

TEST_CLASS(RecipeModifierSource_StatTransferTable, "ArcCraft.Modifier.StatTransferTable")
{
	FArcItemTestRecordFiles ItemRecords;
	FArcCraftTestRecordFiles Records;

	// -----------------------------------------------------------------------
	// Smoke test: verify the record-id wiring pattern works end-to-end.
	// If this fails, all formula tests are meaningless — guard early.
	// -----------------------------------------------------------------------

	/**
	 * Write a stat transfer table record, assign its id to Source.Table, call Evaluate,
	 * and confirm exactly one modifier comes back.
	 * Validates the record-file resolve path that all other tests rely on.
	 * The entry's tag query targets Wood; the spec carries Wood in its AssetTags.
	 */
	TEST_METHOD(RecordId_ResolvesToIndexedTable)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 1.0f, 2.0f, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, WoodTag, 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients     = { Spec };
		TArray<float> SlotRaritySteps           = { 1.0f };
		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});

		ASSERT_THAT(AreEqual(1, Results.Num(),
			TEXT("Record smoke: single-entry table must produce exactly 1 modifier")));
	}

	// -----------------------------------------------------------------------
	// Ported formula / distribution tests (one entry per table, matching the
	// old single-source tests' math; the rolls draw from a seeded Context.Stream).
	// The context has no OutputRarity, so ArcCraftRarity::ComputeBandValue rolls the
	// full range, biased by the entry's RBias and the slot step only.
	// -----------------------------------------------------------------------

	/**
	 * Q=2.0, RBias=1.0, range [0.5, 1.5].
	 * Exp = 1/(1+1*(2-1)) = 0.5.  E[t] = 1/(0.5+1) = 2/3.
	 * E[Roll] = 0.5 + (2/3)*1.0 = 1.167.
	 * Assert: mean over 200 rolls > 1.05.
	 */
	TEST_METHOD(HighR_BiasesRollTowardMax)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 0.5f, 1.5f, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, WoodTag, 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 2.0f };

		const FArcCraftContext Context = ArcStatTransferTableTestsImpl::MakeSeededContext(SlotRaritySteps, 2.0f, 12345);

		float Sum = 0.0f;
		const int32 Iterations = 200;
		for (int32 i = 0; i < Iterations; ++i)
		{
			TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, Context);
			ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("HighQ: must produce exactly 1 modifier")));
			Sum += Results[0].Result.Stat.Value.GetValue();
		}

		const float Mean = Sum / static_cast<float>(Iterations);
		ASSERT_THAT(IsTrue(Mean > 1.05f,
			FString::Printf(TEXT("HighQ mean %.4f should be > 1.05 (biased toward Max=1.5)"), Mean)));
	}

	/**
	 * Q=0.5, RBias=1.0, range [0.5, 1.5].
	 * Exp = 1/(1+1*(0.5-1)) = 1/0.5 = 2.0.  E[t] = 1/(2+1) = 1/3.
	 * E[Roll] = 0.5 + (1/3)*1.0 = 0.833.
	 * Assert: mean over 200 rolls < 0.95.
	 */
	TEST_METHOD(LowR_BiasesRollTowardMin)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 0.5f, 1.5f, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, WoodTag, 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 0.5f };

		const FArcCraftContext Context = ArcStatTransferTableTestsImpl::MakeSeededContext(SlotRaritySteps, 0.5f, 12345);

		float Sum = 0.0f;
		const int32 Iterations = 200;
		for (int32 i = 0; i < Iterations; ++i)
		{
			TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, Context);
			ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("LowQ: must produce exactly 1 modifier")));
			Sum += Results[0].Result.Stat.Value.GetValue();
		}

		const float Mean = Sum / static_cast<float>(Iterations);
		ASSERT_THAT(IsTrue(Mean < 0.95f,
			FString::Printf(TEXT("LowQ mean %.4f should be < 0.95 (biased toward Min=0.5)"), Mean)));
	}

	/**
	 * Q=1.0, RBias=1.0, range [0.5, 1.5].
	 * Exp = 1.0 => uniform t => E[Roll] = 1.0.
	 * Assert: mean over 200 rolls in [0.9, 1.1].
	 */
	TEST_METHOD(NeutralR_ProducesUniformDistribution)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 0.5f, 1.5f, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, WoodTag, 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 1.0f };

		const FArcCraftContext Context = ArcStatTransferTableTestsImpl::MakeSeededContext(SlotRaritySteps, 1.0f, 12345);

		float Sum = 0.0f;
		const int32 Iterations = 200;
		for (int32 i = 0; i < Iterations; ++i)
		{
			TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, Context);
			ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("NeutralQ: must produce exactly 1 modifier")));
			Sum += Results[0].Result.Stat.Value.GetValue();
		}

		const float Mean = Sum / static_cast<float>(Iterations);
		ASSERT_THAT(IsTrue(Mean >= 0.9f && Mean <= 1.1f,
			FString::Printf(TEXT("NeutralQ mean %.4f should be in [0.9, 1.1] (uniform)"), Mean)));
	}

	/**
	 * Entry's tag query targets None; no ingredient carries that tag.
	 * Evaluate skips the entry and returns empty (single-entry table, so the whole result
	 * set is empty).
	 */
	TEST_METHOD(NoMatchingIngredient_ReturnsEmpty)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 0.5f, 1.5f, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		// Entry requires None — no ingredient carries this tag, so no modifier is emitted.
		FGameplayTagContainer NoneTag;
		NoneTag.AddTag(TAG_StatTransferTest_Ingredient_None.GetTag());

		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, NoneTag, 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 1.0f };

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("No ingredient carries the required tag — must return empty")));
	}

	/**
	 * Ingredient definition has no FArcItemFragment_ItemStatRanges =>
	 * Evaluate skips the entry and returns empty.
	 */
	TEST_METHOD(NoStatRangesFragment_ReturnsEmpty)
	{
		FArcItemDefinitionData Def;

		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemFragment_Tags TagsFragment;
		TagsFragment.AssetTags = WoodTag;
		ArcItemTestRecords::AddFragment(Def, TagsFragment);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(ItemRecords.Write(Def));
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		Spec.Amount = 1;

		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, WoodTag, 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 1.0f };

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Definition without FArcItemFragment_ItemStatRanges must return empty")));
	}

	/**
	 * ResultType==Stat, and the rolled value is clamped within [Min, Max].
	 */
	TEST_METHOD(ResultIsStatAndInRange)
	{
		const float RangeMin = 1.0f;
		const float RangeMax = 2.0f;

		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, RangeMin, RangeMax, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, WoodTag, 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 1.0f };

		const FArcCraftContext Context = ArcStatTransferTableTestsImpl::MakeSeededContext(SlotRaritySteps, 1.0f, 99999);

		const int32 Iterations = 50;
		for (int32 i = 0; i < Iterations; ++i)
		{
			TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, Context);
			ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Must emit exactly 1 modifier")));

			ASSERT_THAT(AreEqual(EArcCraftModifierResultType::Stat, Results[0].Result.Type,
				TEXT("Result type must be Stat")));

			const float RolledValue = Results[0].Result.Stat.Value.GetValue();
			ASSERT_THAT(IsTrue(RolledValue >= RangeMin && RolledValue <= RangeMax,
				FString::Printf(TEXT("Rolled %.4f out of [%.1f, %.1f]"), RolledValue, RangeMin, RangeMax)));
		}
	}

	// -----------------------------------------------------------------------
	// New tests: multi-entry and error-recovery behaviour.
	// -----------------------------------------------------------------------

	/**
	 * Three entries, each with a distinct tag query (Wood, Stone, Iron).
	 * Three specs carrying the matching tags — one per query.
	 * Expect exactly 3 modifiers.
	 */
	TEST_METHOD(MultiEntryTable_ProducesOneModifierPerValidEntry)
	{
		FArcStatTransferTableData Table;

		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FGameplayTagContainer StoneTag;
		StoneTag.AddTag(TAG_StatTransferTest_Ingredient_Stone.GetTag());

		FGameplayTagContainer IronTag;
		IronTag.AddTag(TAG_StatTransferTest_Ingredient_Iron.GetTag());

		FArcItemSpec Spec0 = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 1.0f, 2.0f, WoodTag);
		ASSERT_THAT(IsTrue(Spec0.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec Spec1 = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 2.0f, 3.0f, StoneTag);
		ASSERT_THAT(IsTrue(Spec1.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec Spec2 = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 3.0f, 4.0f, IronTag);
		ASSERT_THAT(IsTrue(Spec2.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcAttributeRef StatRef = ArcStatTransferTableTestsImpl::MakeTestStatRef();

		FArcStatTransferEntry Entry0;
		Entry0.IngredientQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(WoodTag);
		Entry0.Stat                = StatRef;
		Entry0.RBias         = 1.0f;
		Entry0.OutputModType       = EArcModType::Additive;

		FArcStatTransferEntry Entry1;
		Entry1.IngredientQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(StoneTag);
		Entry1.Stat                = StatRef;
		Entry1.RBias         = 1.0f;
		Entry1.OutputModType       = EArcModType::Additive;

		FArcStatTransferEntry Entry2;
		Entry2.IngredientQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(IronTag);
		Entry2.Stat                = StatRef;
		Entry2.RBias         = 1.0f;
		Entry2.OutputModType       = EArcModType::Additive;

		Table.Entries.Add(Entry0);
		Table.Entries.Add(Entry1);
		Table.Entries.Add(Entry2);

		FArcRecipeModifierSource_StatTransferTable Source;
		Source.Table = Records.Write(Table);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec0, Spec1, Spec2 };
		TArray<float> SlotRaritySteps       = { 1.0f, 1.0f, 1.0f };

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});
		ASSERT_THAT(AreEqual(3, Results.Num(),
			TEXT("Three valid entries must produce exactly 3 modifiers")));
	}

	/**
	 * Three entries. Middle entry's tag query targets None — no ingredient carries it.
	 * Two flanking entries target Wood and Iron respectively.
	 * Two specs (Wood, Iron). Expect 2 modifiers; the bad middle row is silently skipped.
	 */
	TEST_METHOD(BadEntryDoesNotKillSiblings)
	{
		FArcStatTransferTableData Table;

		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FGameplayTagContainer IronTag;
		IronTag.AddTag(TAG_StatTransferTest_Ingredient_Iron.GetTag());

		FGameplayTagContainer NoneTag;
		NoneTag.AddTag(TAG_StatTransferTest_Ingredient_None.GetTag());

		FArcItemSpec Spec0 = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 1.0f, 2.0f, WoodTag);
		ASSERT_THAT(IsTrue(Spec0.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec Spec2 = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 3.0f, 4.0f, IronTag);
		ASSERT_THAT(IsTrue(Spec2.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcAttributeRef StatRef = ArcStatTransferTableTestsImpl::MakeTestStatRef();

		FArcStatTransferEntry EntryGood0;
		EntryGood0.IngredientQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(WoodTag);
		EntryGood0.Stat                = StatRef;
		EntryGood0.RBias         = 1.0f;
		EntryGood0.OutputModType       = EArcModType::Additive;

		FArcStatTransferEntry EntryBad;
		EntryBad.IngredientQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(NoneTag);
		EntryBad.Stat                = StatRef;
		EntryBad.RBias         = 1.0f;
		EntryBad.OutputModType       = EArcModType::Additive;

		FArcStatTransferEntry EntryGood1;
		EntryGood1.IngredientQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(IronTag);
		EntryGood1.Stat                = StatRef;
		EntryGood1.RBias         = 1.0f;
		EntryGood1.OutputModType       = EArcModType::Additive;

		Table.Entries.Add(EntryGood0);
		Table.Entries.Add(EntryBad);
		Table.Entries.Add(EntryGood1);

		FArcRecipeModifierSource_StatTransferTable Source;
		Source.Table = Records.Write(Table);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec0, Spec2 };
		TArray<float> SlotRaritySteps       = { 1.0f, 1.0f };

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});
		ASSERT_THAT(AreEqual(2, Results.Num(),
			TEXT("Bad middle entry must be skipped; 2 valid siblings must still produce 2 modifiers")));
	}

	/**
	 * Source.Table is default-constructed (invalid id).
	 * Evaluate must return an empty array and must not crash.
	 * (A UE_LOG warning is expected; we do not assert on log output.)
	 */
	TEST_METHOD(InvalidId_ReturnsEmptyAndDoesNotCrash)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 0.5f, 1.5f, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcRecipeModifierSource_StatTransferTable Source;

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 1.0f };

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Invalid Table id must return empty without crashing")));
	}

	/**
	 * Source.Table holds a well-formed id that no indexed record carries.
	 * Evaluate must return an empty array and must not crash.
	 */
	TEST_METHOD(UnresolvedId_ReturnsEmptyAndDoesNotCrash)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 0.5f, 1.5f, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcRecipeModifierSource_StatTransferTable Source;
		Source.Table = FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcStatTransferTable")), FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)));

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 1.0f };

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Unresolved Table id must return empty without crashing")));
	}

	// -----------------------------------------------------------------------
	// Additional coverage: multi-match and inert-row semantics.
	// -----------------------------------------------------------------------

	/**
	 * Two ingredients both carry the same tag. A single entry's tag query matches both.
	 * Resolver semantics: one source can emit N pending modifiers (per the
	 * FArcRecipeModifierSource_MaterialProperties precedent). Assert N=2.
	 */
	TEST_METHOD(MultipleIngredientsMatchSameQuery_ProducesOneModifierPerMatch)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec0 = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 1.0f, 2.0f, WoodTag);
		ASSERT_THAT(IsTrue(Spec0.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec Spec1 = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 1.0f, 2.0f, WoodTag);
		ASSERT_THAT(IsTrue(Spec1.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, WoodTag, 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec0, Spec1 };
		TArray<float> SlotRaritySteps       = { 1.0f, 1.0f };

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});
		ASSERT_THAT(AreEqual(2, Results.Num(),
			TEXT("Two ingredients matching the same tag query must produce 2 pending modifiers")));
	}

	/**
	 * Default (empty) IngredientQuery is intentionally inert per the field contract.
	 * No modifiers must be emitted, regardless of consumed ingredient tags.
	 */
	TEST_METHOD(EmptyQuery_TreatedAsInertRow)
	{
		FGameplayTagContainer WoodTag;
		WoodTag.AddTag(TAG_StatTransferTest_Ingredient_Wood.GetTag());

		FArcItemSpec Spec = ArcStatTransferTableTestsImpl::MakeSpecWithRange(ItemRecords, 1.0f, 2.0f, WoodTag);
		ASSERT_THAT(IsTrue(Spec.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));

		// Empty FGameplayTagContainer -> MakeSourceWithSingleEntry stores default (empty) query.
		FArcRecipeModifierSource_StatTransferTable Source =
			ArcStatTransferTableTestsImpl::MakeSourceWithSingleEntry(
				Records, FGameplayTagContainer(), 1.0f, EArcModType::Additive);
		ASSERT_THAT(IsTrue(Source.Table.IsValid(), TEXT("Stat transfer table record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = { Spec };
		TArray<float> SlotRaritySteps       = { 1.0f };

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Empty IngredientQuery must produce zero modifiers (inert row)")));
	}
};
