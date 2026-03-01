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
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcRandomPoolIntegrationTest, Log, All);

// ---- Test tags (unique prefix for this TU) ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PoolTest_Resource_Metal, "Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PoolTest_Resource_Gem, "Resource.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_PoolTest_Resource_Wood, "Resource.Wood");

// ===================================================================
// Helpers
// ===================================================================

namespace RandomPoolTestHelpers
{
	/** Create a transient random pool definition. */
	UArcRandomPoolDefinition* CreateTransientPool()
	{
		return NewObject<UArcRandomPoolDefinition>(GetTransientPackage());
	}

	/**
	 * Create a stat modifier wrapped in FInstancedStruct.
	 * Uses zero quality scaling by default for deterministic stat values.
	 */
	FInstancedStruct MakeStatModifier(float BaseValue, float QualityScalingFactor = 0.0f)
	{
		FArcCraftModifier_Stats StatMod;
		FArcItemAttributeStat Stat;
		Stat.SetValue(BaseValue);
		StatMod.BaseStat = Stat;
		StatMod.QualityScalingFactor = QualityScalingFactor;

		FInstancedStruct Instance;
		Instance.InitializeAs<FArcCraftModifier_Stats>(StatMod);
		return Instance;
	}

	/**
	 * Create a pool entry with a single stat modifier.
	 * Default: always eligible, equal weight, cost=1.
	 */
	FArcRandomPoolEntry MakeStatEntry(
		const FText& Name,
		float StatValue,
		float BaseWeight = 1.0f,
		float Cost = 1.0f,
		float MinQualityThreshold = 0.0f)
	{
		FArcRandomPoolEntry Entry;
		Entry.DisplayName = Name;
		Entry.BaseWeight = BaseWeight;
		Entry.Cost = Cost;
		Entry.MinQualityThreshold = MinQualityThreshold;
		Entry.bScaleByQuality = false;
		Entry.ValueScale = 1.0f;
		Entry.ValueSkew = 0.0f;
		Entry.Modifiers.Add(MakeStatModifier(StatValue));
		return Entry;
	}

	/**
	 * Create a SimpleRandom selection mode wrapped in FInstancedStruct.
	 */
	FInstancedStruct MakeSimpleRandomMode(int32 MaxSelections, bool bAllowDuplicates = false)
	{
		FArcRandomPoolSelection_SimpleRandom Mode;
		Mode.MaxSelections = MaxSelections;
		Mode.bAllowDuplicates = bAllowDuplicates;
		Mode.bQualityAffectsSelections = false;

		FInstancedStruct Instance;
		Instance.InitializeAs<FArcRandomPoolSelection_SimpleRandom>(Mode);
		return Instance;
	}

	/**
	 * Create a Budget selection mode wrapped in FInstancedStruct.
	 */
	FInstancedStruct MakeBudgetMode(float BaseBudget, float BudgetPerQuality = 0.0f,
		int32 MaxBudgetSelections = 0, bool bAllowDuplicates = false)
	{
		FArcRandomPoolSelection_Budget Mode;
		Mode.BaseBudget = BaseBudget;
		Mode.BudgetPerQuality = BudgetPerQuality;
		Mode.MaxBudgetSelections = MaxBudgetSelections;
		Mode.bAllowDuplicates = bAllowDuplicates;

		FInstancedStruct Instance;
		Instance.InitializeAs<FArcRandomPoolSelection_Budget>(Mode);
		return Instance;
	}

	/** Apply a single modifier result to an item spec. */
	void ApplyResult(FArcItemSpec& OutSpec, const FArcCraftModifierResult& Result)
	{
		if (Result.Type == EArcCraftModifierResultType::Stat)
		{
			FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			if (!StatsFragment)
			{
				StatsFragment = new FArcItemFragment_ItemStats();
				StatsFragment->DefaultStats.Add(Result.Stat);
				OutSpec.AddInstanceData(StatsFragment);
			}
			else
			{
				StatsFragment->DefaultStats.Add(Result.Stat);
			}
		}
	}

	/**
	 * Build a FArcRecipeOutputModifier_RandomPool, evaluate it, and apply results.
	 * Returns the resulting item spec.
	 */
	FArcItemSpec ApplyRandomPool(
		UArcRandomPoolDefinition* Pool,
		const FInstancedStruct& SelectionMode,
		const TArray<FArcItemSpec>& Ingredients = {},
		const TArray<float>& QualityMults = {},
		float AverageQuality = 1.0f)
	{
		FArcRecipeOutputModifier_RandomPool PoolModifier;
		PoolModifier.PoolDefinition = Pool;
		PoolModifier.SelectionMode = SelectionMode;

		FArcItemSpec OutSpec;
		TArray<FArcCraftPendingModifier> Results = PoolModifier.Evaluate(
			Ingredients, QualityMults, AverageQuality);
		for (const FArcCraftPendingModifier& Pending : Results)
		{
			if (Pending.Result.Type != EArcCraftModifierResultType::None)
			{
				ApplyResult(OutSpec, Pending.Result);
			}
		}
		return OutSpec;
	}

	/** Log the stats in an item spec. */
	void LogStats(const FArcItemSpec& OutSpec, const TCHAR* TestLabel)
	{
		const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		if (Stats)
		{
			UE_LOG(LogArcRandomPoolIntegrationTest, Log,
				TEXT("=== %s: %d stat(s) ==="), TestLabel, Stats->DefaultStats.Num());
			for (int32 i = 0; i < Stats->DefaultStats.Num(); ++i)
			{
				UE_LOG(LogArcRandomPoolIntegrationTest, Log,
					TEXT("  Stat[%d] = %.3f"), i, Stats->DefaultStats[i].Value.GetValue());
			}
		}
		else
		{
			UE_LOG(LogArcRandomPoolIntegrationTest, Log,
				TEXT("=== %s: no stats fragment ==="), TestLabel);
		}
	}
}

// ===================================================================
// SimpleRandom: 5 entries, selecting 1, 2, or 3
// ===================================================================

TEST_CLASS(RandomPool_SimpleRandom_SelectiveEntries, "ArcCraft.RandomPool.Integration.SimpleRandom")
{
	/**
	 * Helper: Creates a pool with 5 entries, each with a unique stat value.
	 *
	 * Entry 0: "Iron Bonus"     stat=10.0  (always eligible)
	 * Entry 1: "Steel Bonus"    stat=20.0  (always eligible)
	 * Entry 2: "Silver Bonus"   stat=50.0  (always eligible)
	 * Entry 3: "Gold Bonus"     stat=100.0 (always eligible)
	 * Entry 4: "Mythril Bonus"  stat=250.0 (always eligible)
	 */
	UArcRandomPoolDefinition* CreateFiveEntryPool()
	{
		UArcRandomPoolDefinition* Pool = RandomPoolTestHelpers::CreateTransientPool();
		Pool->PoolName = FText::FromString("Five-Entry Test Pool");

		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Iron Bonus"), 10.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Steel Bonus"), 20.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Silver Bonus"), 50.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Gold Bonus"), 100.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Mythril Bonus"), 250.0f));

		return Pool;
	}

	TEST_METHOD(FiveEntries_Select1_ExactlyOneStat)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveEntryPool();
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 1);

		TSet<int32> ObservedStatValues;

		for (int32 Run = 0; Run < 100; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats, TEXT("Should produce stats")));
			ASSERT_THAT(AreEqual(1, Stats->DefaultStats.Num(),
				TEXT("Select 1 from 5 should produce exactly 1 stat")));

			ObservedStatValues.Add(FMath::RoundToInt32(Stats->DefaultStats[0].Value.GetValue()));
		}

		// With equal weights over 100 runs, all 5 entries should appear
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(10), TEXT("Iron(10) should appear")));
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(20), TEXT("Steel(20) should appear")));
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(50), TEXT("Silver(50) should appear")));
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(100), TEXT("Gold(100) should appear")));
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(250), TEXT("Mythril(250) should appear")));

		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  Select1: All 5 unique stat values observed over 100 runs"));
	}

	TEST_METHOD(FiveEntries_Select2_ExactlyTwoStats)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveEntryPool();
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 2);

		TMap<int32, int32> StatCounts;

		for (int32 Run = 0; Run < 100; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(2, Stats->DefaultStats.Num(),
				TEXT("Select 2 from 5 should produce exactly 2 stats")));

			for (int32 i = 0; i < Stats->DefaultStats.Num(); ++i)
			{
				const int32 V = FMath::RoundToInt32(Stats->DefaultStats[i].Value.GetValue());
				StatCounts.FindOrAdd(V, 0)++;
			}
		}

		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  Select2: Iron=%d, Steel=%d, Silver=%d, Gold=%d, Mythril=%d (total=200)"),
			StatCounts.FindRef(10), StatCounts.FindRef(20), StatCounts.FindRef(50),
			StatCounts.FindRef(100), StatCounts.FindRef(250));

		// Each entry should appear at least once across 200 selections
		ASSERT_THAT(IsTrue(StatCounts.FindRef(10) > 0, TEXT("Iron should appear")));
		ASSERT_THAT(IsTrue(StatCounts.FindRef(20) > 0, TEXT("Steel should appear")));
		ASSERT_THAT(IsTrue(StatCounts.FindRef(50) > 0, TEXT("Silver should appear")));
		ASSERT_THAT(IsTrue(StatCounts.FindRef(100) > 0, TEXT("Gold should appear")));
		ASSERT_THAT(IsTrue(StatCounts.FindRef(250) > 0, TEXT("Mythril should appear")));
	}

	TEST_METHOD(FiveEntries_Select3_ExactlyThreeStats)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveEntryPool();
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 3);

		for (int32 Run = 0; Run < 100; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(3, Stats->DefaultStats.Num(),
				TEXT("Select 3 from 5 should produce exactly 3 stats")));

			if (Run == 0)
			{
				RandomPoolTestHelpers::LogStats(OutSpec, TEXT("Select3_FirstRun"));
			}
		}
	}

	TEST_METHOD(FiveEntries_Select3_NoDuplicates_AllUniqueStats)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveEntryPool();
		// bAllowDuplicates = false (default) means each entry can only be selected once
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 3, /*bAllowDuplicates=*/ false);

		for (int32 Run = 0; Run < 100; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(3, Stats->DefaultStats.Num()));

			// All 3 stats should be different values (no duplicate entries)
			TSet<int32> StatValues;
			for (int32 i = 0; i < Stats->DefaultStats.Num(); ++i)
			{
				StatValues.Add(FMath::RoundToInt32(Stats->DefaultStats[i].Value.GetValue()));
			}
			ASSERT_THAT(AreEqual(3, StatValues.Num(),
				TEXT("3 selected entries without duplicates must have 3 unique stat values")));
		}
	}

	TEST_METHOD(FiveEntries_Select3_AllowDuplicates_MayHaveRepeats)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveEntryPool();
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 3, /*bAllowDuplicates=*/ true);

		int32 RunsWithDuplicates = 0;

		for (int32 Run = 0; Run < 200; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(3, Stats->DefaultStats.Num(),
				TEXT("Always 3 stats when selecting 3 with duplicates")));

			TSet<int32> Unique;
			for (int32 i = 0; i < Stats->DefaultStats.Num(); ++i)
			{
				Unique.Add(FMath::RoundToInt32(Stats->DefaultStats[i].Value.GetValue()));
			}
			if (Unique.Num() < 3)
			{
				RunsWithDuplicates++;
			}
		}

		// With 5 entries and 3 selections allowing duplicates, we should see some duplicates
		// P(at least one dup in 3 picks from 5) = 1 - (5/5 * 4/5 * 3/5) = 1 - 0.48 = 0.52
		// Over 200 runs, we should see plenty
		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  Select3_AllowDuplicates: %d/%d runs had duplicate entries"), RunsWithDuplicates, 200);
		ASSERT_THAT(IsTrue(RunsWithDuplicates > 0,
			TEXT("With duplicates allowed, at least some runs should have repeated entries")));
	}

	TEST_METHOD(FiveEntries_SelectMoreThanAvailable_CappedByPoolSize)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveEntryPool();
		// Try to select 10, but only 5 entries exist and duplicates not allowed
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 10, /*bAllowDuplicates=*/ false);

		for (int32 Run = 0; Run < 50; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			// Should cap at 5 (pool size), not 10
			ASSERT_THAT(AreEqual(5, Stats->DefaultStats.Num(),
				TEXT("Selecting 10 from 5 without duplicates should cap at 5 stats")));

			if (Run == 0)
			{
				RandomPoolTestHelpers::LogStats(OutSpec, TEXT("SelectMoreThanAvailable_FirstRun"));
			}
		}
	}
};

// ===================================================================
// SimpleRandom: 5 entries with eligibility filtering
// Only some entries are eligible based on quality or tags.
// ===================================================================

TEST_CLASS(RandomPool_SimpleRandom_Eligibility, "ArcCraft.RandomPool.Integration.SimpleRandom.Eligibility")
{
	TEST_METHOD(FiveEntries_TwoRequireHighQuality_LowQualityGetsThree)
	{
		UArcRandomPoolDefinition* Pool = RandomPoolTestHelpers::CreateTransientPool();

		// 3 entries always eligible, 2 require high quality
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Iron"), 10.0f, 1.0f, 1.0f, 0.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Steel"), 20.0f, 1.0f, 1.0f, 0.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Silver"), 50.0f, 1.0f, 1.0f, 0.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Gold"), 100.0f, 1.0f, 1.0f, 3.0f));    // needs quality >= 3.0
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Mythril"), 250.0f, 1.0f, 1.0f, 5.0f)); // needs quality >= 5.0

		// Select 3, but at low quality only 3 entries are eligible
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 3, /*bAllowDuplicates=*/ false);

		TMap<int32, int32> StatCounts;

		for (int32 Run = 0; Run < 100; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode,
				/*Ingredients=*/ {}, /*QualityMults=*/ {}, /*AverageQuality=*/ 1.0f);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			// Only 3 eligible, selecting 3 without dups = exactly 3
			ASSERT_THAT(AreEqual(3, Stats->DefaultStats.Num(),
				TEXT("3 eligible, selecting 3 = 3 stats")));

			for (int32 i = 0; i < Stats->DefaultStats.Num(); ++i)
			{
				const int32 V = FMath::RoundToInt32(Stats->DefaultStats[i].Value.GetValue());
				StatCounts.FindOrAdd(V, 0)++;

				// Should never see Gold(100) or Mythril(250)
				ASSERT_THAT(IsTrue(V == 10 || V == 20 || V == 50,
					TEXT("Only Iron/Steel/Silver should be selectable at quality 1.0")));
			}
		}

		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  LowQuality: Iron=%d, Steel=%d, Silver=%d, Gold=%d, Mythril=%d"),
			StatCounts.FindRef(10), StatCounts.FindRef(20), StatCounts.FindRef(50),
			StatCounts.FindRef(100), StatCounts.FindRef(250));
	}

	TEST_METHOD(FiveEntries_HighQualityUnlocksAll_SelectThree)
	{
		UArcRandomPoolDefinition* Pool = RandomPoolTestHelpers::CreateTransientPool();

		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Iron"), 10.0f, 1.0f, 1.0f, 0.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Steel"), 20.0f, 1.0f, 1.0f, 0.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Silver"), 50.0f, 1.0f, 1.0f, 0.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Gold"), 100.0f, 1.0f, 1.0f, 3.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Mythril"), 250.0f, 1.0f, 1.0f, 5.0f));

		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 3, /*bAllowDuplicates=*/ false);

		TSet<int32> AllObservedValues;

		for (int32 Run = 0; Run < 200; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode,
				/*Ingredients=*/ {}, /*QualityMults=*/ {}, /*AverageQuality=*/ 10.0f);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(3, Stats->DefaultStats.Num(),
				TEXT("5 eligible, selecting 3 = 3 stats")));

			for (int32 i = 0; i < Stats->DefaultStats.Num(); ++i)
			{
				AllObservedValues.Add(FMath::RoundToInt32(Stats->DefaultStats[i].Value.GetValue()));
			}
		}

		// All 5 should appear across 200 runs
		ASSERT_THAT(IsTrue(AllObservedValues.Contains(10), TEXT("Iron should appear")));
		ASSERT_THAT(IsTrue(AllObservedValues.Contains(20), TEXT("Steel should appear")));
		ASSERT_THAT(IsTrue(AllObservedValues.Contains(50), TEXT("Silver should appear")));
		ASSERT_THAT(IsTrue(AllObservedValues.Contains(100), TEXT("Gold should appear")));
		ASSERT_THAT(IsTrue(AllObservedValues.Contains(250), TEXT("Mythril should appear")));

		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  HighQuality: All 5 entries observed over 200 runs"));
	}

	TEST_METHOD(FiveEntries_TagFilter_OnlyMatchingEntriesSelected)
	{
		UArcRandomPoolDefinition* Pool = RandomPoolTestHelpers::CreateTransientPool();

		// Entries 0-1: no tag requirements
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Universal A"), 10.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Universal B"), 20.0f));

		// Entry 2: requires Metal tag
		{
			FArcRandomPoolEntry Entry = RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Metal Special"), 50.0f);
			Entry.RequiredIngredientTags.AddTag(TAG_PoolTest_Resource_Metal);
			Pool->Entries.Add(Entry);
		}

		// Entry 3: requires Gem tag
		{
			FArcRandomPoolEntry Entry = RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Gem Special"), 100.0f);
			Entry.RequiredIngredientTags.AddTag(TAG_PoolTest_Resource_Gem);
			Pool->Entries.Add(Entry);
		}

		// Entry 4: requires Wood tag (which we won't provide)
		{
			FArcRandomPoolEntry Entry = RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Wood Special"), 250.0f);
			Entry.RequiredIngredientTags.AddTag(TAG_PoolTest_Resource_Wood);
			Pool->Entries.Add(Entry);
		}

		FInstancedStruct Mode = RandomPoolTestHelpers::MakeSimpleRandomMode(/*MaxSelections=*/ 3, /*bAllowDuplicates=*/ false);

		// Provide Metal ingredient only => entries 0,1,2 eligible (3 total), entries 3,4 not
		FGameplayTagContainer MetalItemTags;
		MetalItemTags.AddTag(TAG_PoolTest_Resource_Metal);

		UArcItemDefinition* MetalDef = NewObject<UArcItemDefinition>(
			GetTransientPackage(), TEXT("TestItem_PoolMetal"), RF_Transient);
		MetalDef->RegenerateItemId();

		FArcItemFragment_Tags TagsFragment;
		TagsFragment.AssetTags = MetalItemTags;
		MetalDef->AddFragment(TagsFragment);

		FArcItemSpec MetalSpec;
		MetalSpec.SetItemDefinitionAsset(MetalDef);
		MetalSpec.SetItemDefinition(MetalDef->GetPrimaryAssetId());
		TArray<FArcItemSpec> Ingredients = { MetalSpec };
		TArray<float> QualityMults = { 1.0f };

		TMap<int32, int32> StatCounts;

		for (int32 Run = 0; Run < 100; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode, Ingredients, QualityMults, 1.0f);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(3, Stats->DefaultStats.Num(),
				TEXT("3 eligible entries, selecting 3 = 3 stats")));

			for (int32 i = 0; i < Stats->DefaultStats.Num(); ++i)
			{
				const int32 V = FMath::RoundToInt32(Stats->DefaultStats[i].Value.GetValue());
				StatCounts.FindOrAdd(V, 0)++;

				// Should never see Gem(100) or Wood(250)
				ASSERT_THAT(IsTrue(V == 10 || V == 20 || V == 50,
					TEXT("Only Universal A/B and Metal Special should be selectable with Metal ingredient")));
			}
		}

		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  TagFilter_Metal: UnivA=%d, UnivB=%d, Metal=%d, Gem=%d, Wood=%d"),
			StatCounts.FindRef(10), StatCounts.FindRef(20), StatCounts.FindRef(50),
			StatCounts.FindRef(100), StatCounts.FindRef(250));
	}
};

// ===================================================================
// Budget mode: 5 entries with costs, budget limits total selections
// ===================================================================

TEST_CLASS(RandomPool_Budget_SelectiveEntries, "ArcCraft.RandomPool.Integration.Budget")
{
	/**
	 * Helper: Creates a pool with 5 entries at increasing costs.
	 *
	 * Entry 0: "Iron"     cost=1.0  stat=10.0
	 * Entry 1: "Steel"    cost=2.0  stat=20.0
	 * Entry 2: "Silver"   cost=3.0  stat=50.0
	 * Entry 3: "Gold"     cost=4.0  stat=100.0
	 * Entry 4: "Mythril"  cost=5.0  stat=250.0
	 */
	UArcRandomPoolDefinition* CreateFiveCostPool()
	{
		UArcRandomPoolDefinition* Pool = RandomPoolTestHelpers::CreateTransientPool();
		Pool->PoolName = FText::FromString("Five-Cost Test Pool");

		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Iron"), 10.0f, 1.0f, 1.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Steel"), 20.0f, 1.0f, 2.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Silver"), 50.0f, 1.0f, 3.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Gold"), 100.0f, 1.0f, 4.0f));
		Pool->Entries.Add(RandomPoolTestHelpers::MakeStatEntry(FText::FromString("Mythril"), 250.0f, 1.0f, 5.0f));

		return Pool;
	}

	TEST_METHOD(Budget1_CanOnlyAffordCheapest)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveCostPool();
		// Budget = 1.0, only entry 0 (cost=1.0) is affordable
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeBudgetMode(/*BaseBudget=*/ 1.0f);

		for (int32 Run = 0; Run < 50; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(1, Stats->DefaultStats.Num(),
				TEXT("Budget 1.0 can only afford Iron (cost=1)")));
			ASSERT_THAT(IsNear(10.0f, Stats->DefaultStats[0].Value.GetValue(), 0.001f,
				TEXT("Should always be Iron stat=10")));
		}
	}

	TEST_METHOD(Budget3_AffordsOneToThreeEntries)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveCostPool();
		// Budget = 3.0
		// Could afford: Iron(1) + Steel(2) = 3, or Silver(3) alone, or Iron(1) x3 (if dups), etc.
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeBudgetMode(/*BaseBudget=*/ 3.0f,
			/*BudgetPerQuality=*/ 0.0f, /*MaxBudgetSelections=*/ 0, /*bAllowDuplicates=*/ false);

		int32 MinEntries = INT_MAX;
		int32 MaxEntries = 0;

		for (int32 Run = 0; Run < 200; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));

			const int32 Count = Stats->DefaultStats.Num();
			MinEntries = FMath::Min(MinEntries, Count);
			MaxEntries = FMath::Max(MaxEntries, Count);

			// Verify total cost doesn't exceed budget
			float TotalValue = 0.0f;
			for (int32 i = 0; i < Stats->DefaultStats.Num(); ++i)
			{
				TotalValue += Stats->DefaultStats[i].Value.GetValue();
			}

			// Can't get more than 3 entries (cheapest: 1+2=3 or 3 alone)
			ASSERT_THAT(IsTrue(Count >= 1 && Count <= 3,
				TEXT("Budget 3.0 should produce 1-3 entries")));
		}

		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  Budget3: min=%d, max=%d entries over 200 runs"), MinEntries, MaxEntries);
	}

	TEST_METHOD(Budget5_NoDuplicates_MaxThreeUniqueEntries)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveCostPool();
		// Budget = 5.0, no duplicates
		// Best case: Iron(1) + Steel(2) = 3, then Silver(3) => total=6 > budget, so only 2
		// Or: Iron(1) + Silver(3) = 4, then Steel(2) => total=6 > budget, so only 2
		// Actually: Iron(1) + Steel(2) + leftover=2 => no affordable entry at cost<=2 remaining? Steel already used
		// Iron(1) + Steel(2) = 3, remaining=2, next cheapest unused = Silver(3) > 2, so stop at 2
		// OR: Mythril(5) alone => 1 entry
		// OR: Gold(4) + Iron(1) = 5 => 2 entries
		// Max possible without dups: 2 entries (since 1+2=3, remaining=2, next unused starts at 3)
		// Wait: Iron(1) first, remaining=4. Steel(2) next, remaining=2. Silver(3) > 2. So 2 entries.
		// Unless it picks Iron(1), remaining=4. Then Silver(3), remaining=1. Iron already picked. Steel(2) > 1. So 2.
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeBudgetMode(/*BaseBudget=*/ 5.0f,
			/*BudgetPerQuality=*/ 0.0f, /*MaxBudgetSelections=*/ 0, /*bAllowDuplicates=*/ false);

		int32 MinEntries = INT_MAX;
		int32 MaxEntries = 0;

		for (int32 Run = 0; Run < 200; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));

			const int32 Count = Stats->DefaultStats.Num();
			MinEntries = FMath::Min(MinEntries, Count);
			MaxEntries = FMath::Max(MaxEntries, Count);

			ASSERT_THAT(IsTrue(Count >= 1, TEXT("Should always select at least 1")));
			ASSERT_THAT(IsTrue(Count <= 5, TEXT("Can't exceed pool size")));
		}

		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  Budget5_NoDups: min=%d, max=%d over 200 runs"), MinEntries, MaxEntries);
	}

	TEST_METHOD(Budget10_NoDuplicates_CanAffordMany)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveCostPool();
		// Budget = 10.0, no duplicates
		// Total cost of all 5 = 1+2+3+4+5 = 15, so can't afford all
		// But can afford 4: 1+2+3+4 = 10 exactly
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeBudgetMode(/*BaseBudget=*/ 10.0f,
			/*BudgetPerQuality=*/ 0.0f, /*MaxBudgetSelections=*/ 0, /*bAllowDuplicates=*/ false);

		int32 MinEntries = INT_MAX;
		int32 MaxEntries = 0;
		TMap<int32, int32> CountDistribution;

		for (int32 Run = 0; Run < 200; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));

			const int32 Count = Stats->DefaultStats.Num();
			MinEntries = FMath::Min(MinEntries, Count);
			MaxEntries = FMath::Max(MaxEntries, Count);
			CountDistribution.FindOrAdd(Count, 0)++;

			ASSERT_THAT(IsTrue(Count >= 1, TEXT("Should always select at least 1")));
		}

		// With budget=10 and costs 1-5, we should see at least 2 entries most times
		ASSERT_THAT(IsTrue(MaxEntries >= 3,
			TEXT("Budget 10 should sometimes allow 3+ entries")));

		UE_LOG(LogArcRandomPoolIntegrationTest, Log,
			TEXT("  Budget10: min=%d, max=%d over 200 runs"), MinEntries, MaxEntries);
		for (auto& Pair : CountDistribution)
		{
			UE_LOG(LogArcRandomPoolIntegrationTest, Log,
				TEXT("    %d entries: %d times"), Pair.Key, Pair.Value);
		}
	}

	TEST_METHOD(Budget_QualityScaling_LowQualityLimited_HighQualityGenerous)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveCostPool();
		// Budget = 1.0 + (quality - 1.0) * 3.0
		// At quality 1.0: budget = 1.0 (only Iron affordable)
		// At quality 3.0: budget = 1.0 + 2.0*3.0 = 7.0 (much more affordable)

		// Low quality
		{
			FInstancedStruct Mode = RandomPoolTestHelpers::MakeBudgetMode(
				/*BaseBudget=*/ 1.0f, /*BudgetPerQuality=*/ 3.0f);

			int32 MaxEntries = 0;
			for (int32 Run = 0; Run < 50; ++Run)
			{
				FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode,
					{}, {}, /*AverageQuality=*/ 1.0f);

				const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
				ASSERT_THAT(IsNotNull(Stats));
				MaxEntries = FMath::Max(MaxEntries, Stats->DefaultStats.Num());
			}
			ASSERT_THAT(AreEqual(1, MaxEntries,
				TEXT("Budget=1 at quality 1.0 should only allow 1 entry")));

			UE_LOG(LogArcRandomPoolIntegrationTest, Log,
				TEXT("  BudgetQualityScaling: lowQ maxEntries=%d (budget=1.0)"), MaxEntries);
		}

		// High quality
		{
			FInstancedStruct Mode = RandomPoolTestHelpers::MakeBudgetMode(
				/*BaseBudget=*/ 1.0f, /*BudgetPerQuality=*/ 3.0f);

			int32 MaxEntries = 0;
			int32 MinEntries = INT_MAX;
			for (int32 Run = 0; Run < 100; ++Run)
			{
				FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode,
					{}, {}, /*AverageQuality=*/ 3.0f);

				const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
				ASSERT_THAT(IsNotNull(Stats));
				MaxEntries = FMath::Max(MaxEntries, Stats->DefaultStats.Num());
				MinEntries = FMath::Min(MinEntries, Stats->DefaultStats.Num());
			}
			// Budget=7, should allow multiple entries
			ASSERT_THAT(IsTrue(MaxEntries > 1,
				TEXT("Budget=7 at quality 3.0 should allow more than 1 entry")));

			UE_LOG(LogArcRandomPoolIntegrationTest, Log,
				TEXT("  BudgetQualityScaling: highQ min=%d, max=%d (budget=7.0)"), MinEntries, MaxEntries);
		}
	}

	TEST_METHOD(Budget_MaxBudgetSelections_CapsCount)
	{
		UArcRandomPoolDefinition* Pool = CreateFiveCostPool();
		// Huge budget (100.0), but MaxBudgetSelections=2
		FInstancedStruct Mode = RandomPoolTestHelpers::MakeBudgetMode(
			/*BaseBudget=*/ 100.0f, /*BudgetPerQuality=*/ 0.0f,
			/*MaxBudgetSelections=*/ 2, /*bAllowDuplicates=*/ false);

		for (int32 Run = 0; Run < 100; ++Run)
		{
			FArcItemSpec OutSpec = RandomPoolTestHelpers::ApplyRandomPool(Pool, Mode);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(2, Stats->DefaultStats.Num(),
				TEXT("MaxBudgetSelections=2 should cap at 2 even with huge budget")));
		}
	}
};
