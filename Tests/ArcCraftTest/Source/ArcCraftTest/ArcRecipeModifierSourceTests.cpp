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
#include "StructUtils/InstancedStruct.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "ArcCraft/Static/ArcStaticModifierSetData.h"
#include "Items/ArcItemSpec.h"

#include "ArcCraftTestRecordFiles.h"
#include "ArcCraftTestRulesFixture.h"

// ---- Tags for modifier source tests (unique prefix) ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ModSrcTest_Offense, "Test.Modifier.Source.Offense");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ModSrcTest_Defense, "Test.Modifier.Source.Defense");

// ===================================================================
// FArcRecipeModifierSource_RandomPool smoke tests
// ===================================================================

TEST_CLASS(RecipeModifierSource_RandomPool, "ArcCraft.Source.Sources")
{
	FArcCraftTestRecordFiles Records;
	FArcCraftTestRulesFixture RulesFixture;

	/**
	 * Source with MinAverageR=2.0, AverageR=1.0.
	 * Eligibility gate: AverageR < MinAverageR => Evaluate must return empty.
	 */
	TEST_METHOD(RandomPoolSource_EvaluateBelowMinAverageR_ReturnsEmpty)
	{
		// The catalog makes the entry convertible, so an empty result can only come from the MinAverageR gate.
		ASSERT_THAT(IsTrue(RulesFixture.Install(Records, { FArcCraftTestRulesFixture::MakeEntry(TEXT("test_stat"), 1.0f, 2.0f) }),
			TEXT("Stat catalog and craft rules records must be written and installed")));

		FArcRandomPoolDefinitionData PoolDef;

		// One eligible entry with a stat modifier
		FArcRandomPoolEntry Entry;
		Entry.DisplayName = FText::FromString("Test Entry");
		Entry.BaseWeight = 1.0f;
		Entry.MinR = 0.0f;
		{
			FArcCraftModifier_Stats StatMod;
			StatMod.StatId = TEXT("test_stat");
			StatMod.SlotTags.AddTag(TAG_ModSrcTest_Offense);
			FInstancedStruct ModStruct;
			ModStruct.InitializeAs<FArcCraftModifier_Stats>(StatMod);
			Entry.Modifiers.Add(MoveTemp(ModStruct));
		}
		PoolDef.Entries.Add(Entry);

		const FPrimaryAssetId PoolId = Records.Write(PoolDef);
		ASSERT_THAT(IsTrue(PoolId.IsValid(), TEXT("Pool record must be written and indexed")));
		ASSERT_THAT(IsNotNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcRandomPoolDefinitionData>(PoolId),
			TEXT("Indexed pool id must resolve to its record")));

		FArcRecipeModifierSource_RandomPool Source;
		Source.MinAverageR = 2.0f;
		Source.PoolDefinition = FArcNamedPrimaryAssetId(PoolId);

		// SelectionMode: SimpleRandom(MaxSelections=1)
		{
			FArcRandomPoolSelection_SimpleRandom SimpleMode;
			SimpleMode.MaxSelections = 1;
			Source.SelectionMode.InitializeAs<FArcRandomPoolSelection_SimpleRandom>(SimpleMode);
		}

		TArray<FArcItemSpec> EmptyIngredients;
		TArray<float> EmptyRaritySteps;
		const float AverageR = 1.0f; // Below MinAverageR=2.0

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(EmptyIngredients, FArcCraftContext{EmptyRaritySteps, AverageR});

		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Source with MinAverageR=2.0 at AverageR=1.0 should return empty")));
	}

	/**
	 * Source with MinAverageR=0, pool with one stat entry.
	 * Evaluate at AverageR=1.5 => returns >=1 pending modifier carrying the modifier's SlotTags.
	 */
	TEST_METHOD(RandomPoolSource_EvaluateAboveMinAverageR_EmitsCandidates)
	{
		ASSERT_THAT(IsTrue(RulesFixture.Install(Records, { FArcCraftTestRulesFixture::MakeEntry(TEXT("test_stat"), 1.0f, 2.0f) }),
			TEXT("Stat catalog and craft rules records must be written and installed")));

		FArcRandomPoolDefinitionData PoolDef;

		FArcRandomPoolEntry Entry;
		Entry.DisplayName = FText::FromString("Strength Entry");
		Entry.BaseWeight = 1.0f;
		Entry.MinR = 0.0f;
		{
			FArcCraftModifier_Stats StatMod;
			StatMod.StatId = TEXT("test_stat");
			StatMod.SlotTags.AddTag(TAG_ModSrcTest_Defense);
			FInstancedStruct ModStruct;
			ModStruct.InitializeAs<FArcCraftModifier_Stats>(StatMod);
			Entry.Modifiers.Add(MoveTemp(ModStruct));
		}
		PoolDef.Entries.Add(Entry);

		const FPrimaryAssetId PoolId = Records.Write(PoolDef);
		ASSERT_THAT(IsTrue(PoolId.IsValid(), TEXT("Pool record must be written and indexed")));
		ASSERT_THAT(IsNotNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcRandomPoolDefinitionData>(PoolId),
			TEXT("Indexed pool id must resolve to its record")));

		FArcRecipeModifierSource_RandomPool Source;
		Source.MinAverageR = 0.0f;
		Source.PoolDefinition = FArcNamedPrimaryAssetId(PoolId);

		{
			FArcRandomPoolSelection_SimpleRandom SimpleMode;
			SimpleMode.MaxSelections = 1;
			Source.SelectionMode.InitializeAs<FArcRandomPoolSelection_SimpleRandom>(SimpleMode);
		}

		TArray<FArcItemSpec> EmptyIngredients;
		TArray<float> EmptyRaritySteps;
		const float AverageR = 1.5f;

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(EmptyIngredients, FArcCraftContext{EmptyRaritySteps, AverageR});

		ASSERT_THAT(IsTrue(Results.Num() >= 1,
			TEXT("RandomPool with one eligible entry should emit at least one pending modifier")));

		// All emitted modifiers must carry the producing modifier's SlotTags
		for (const FArcCraftPendingModifier& Pending : Results)
		{
			ASSERT_THAT(IsTrue(Pending.SlotTags.HasTagExact(TAG_ModSrcTest_Defense.GetTag()),
				TEXT("SlotTags on pending modifier must contain the modifier's slot tag")));
		}
	}
};

// ===================================================================
// FArcRecipeModifierSource_Static smoke test
// ===================================================================

TEST_CLASS(RecipeModifierSource_Static, "ArcCraft.Source.Sources")
{
	FArcCraftTestRecordFiles Records;
	FArcCraftTestRulesFixture RulesFixture;

	/**
	 * Source pointing at a static modifier set record with 2 stat modifiers.
	 * Evaluate must return exactly 2 pending modifiers, both carrying their modifier's SlotTags.
	 */
	TEST_METHOD(StaticSource_Evaluate_EmitsAllModifiers)
	{
		ASSERT_THAT(IsTrue(RulesFixture.Install(Records, { FArcCraftTestRulesFixture::MakeEntry(TEXT("test_stat"), 1.0f, 2.0f) }),
			TEXT("Stat catalog and craft rules records must be written and installed")));

		FArcStaticModifierSetData ModSet;

		// Add two stat modifiers of the catalog stat
		{
			FArcCraftModifier_Stats StatMod1;
			StatMod1.StatId = TEXT("test_stat");
			StatMod1.SlotTags.AddTag(TAG_ModSrcTest_Offense);
			FInstancedStruct ModStruct1;
			ModStruct1.InitializeAs<FArcCraftModifier_Stats>(StatMod1);
			ModSet.Modifiers.Add(MoveTemp(ModStruct1));
		}
		{
			FArcCraftModifier_Stats StatMod2;
			StatMod2.StatId = TEXT("test_stat");
			StatMod2.SlotTags.AddTag(TAG_ModSrcTest_Offense);
			FInstancedStruct ModStruct2;
			ModStruct2.InitializeAs<FArcCraftModifier_Stats>(StatMod2);
			ModSet.Modifiers.Add(MoveTemp(ModStruct2));
		}

		const FPrimaryAssetId SetId = Records.Write(ModSet);
		ASSERT_THAT(IsTrue(SetId.IsValid(), TEXT("Modifier set record must be written and indexed")));

		FArcRecipeModifierSource_Static Source;
		Source.MinAverageR = 0.0f;
		Source.ModifierSet = FArcNamedPrimaryAssetId(SetId);

		TArray<FArcItemSpec> EmptyIngredients;
		TArray<float> EmptyRaritySteps;
		const float AverageR = 1.0f;

		TArray<FArcCraftPendingModifier> Results = Source.Evaluate(EmptyIngredients, FArcCraftContext{EmptyRaritySteps, AverageR});

		ASSERT_THAT(AreEqual(2, Results.Num(),
			TEXT("Static source with 2 modifiers should emit exactly 2 pending modifiers")));

		for (const FArcCraftPendingModifier& Pending : Results)
		{
			ASSERT_THAT(IsTrue(Pending.SlotTags.HasTagExact(TAG_ModSrcTest_Offense.GetTag()),
				TEXT("SlotTags on pending modifier must contain the modifier's slot tag")));
		}
	}
};
