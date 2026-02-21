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
#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"

// ---- Test tags ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Metal, "Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Metal_Iron, "Resource.Metal.Iron");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Metal_Steel, "Resource.Metal.Steel");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Gem, "Resource.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Wood, "Resource.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Recipe_Weapon, "Recipe.Weapon");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Recipe_Armor, "Recipe.Armor");

// ===================================================================
// Helper: Build a property table in memory for tests
// ===================================================================

namespace ArcCraftTestHelpers
{
	/** Create a transient UArcMaterialPropertyTable for testing. */
	UArcMaterialPropertyTable* CreateTransientTable()
	{
		return NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
	}

	/** Create a simple quality band with given parameters and no modifiers. */
	FArcMaterialQualityBand MakeBand(const FText& Name, float MinQuality, float BaseWeight, float QualityWeightBias = 0.0f)
	{
		FArcMaterialQualityBand Band;
		Band.BandName = Name;
		Band.MinQuality = MinQuality;
		Band.BaseWeight = BaseWeight;
		Band.QualityWeightBias = QualityWeightBias;
		return Band;
	}

	/** Create a rule that matches any of the given tags. */
	FArcMaterialPropertyRule MakeRuleMatchAny(const FText& Name, const FGameplayTagContainer& Tags, int32 Priority = 0)
	{
		FArcMaterialPropertyRule Rule;
		Rule.RuleName = Name;
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
		Rule.Priority = Priority;
		Rule.MaxContributions = 1;
		return Rule;
	}

	/** Create a rule that requires ALL of the given tags (AND match). */
	FArcMaterialPropertyRule MakeRuleMatchAll(const FText& Name, const FGameplayTagContainer& Tags, int32 Priority = 0)
	{
		FArcMaterialPropertyRule Rule;
		Rule.RuleName = Name;
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(Tags);
		Rule.Priority = Priority;
		Rule.MaxContributions = 1;
		return Rule;
	}

	/** Build a minimal context with tags and quality, no items involved.
	 *  IngredientTags are placed into a single PerSlotTags entry (simulating one slot). */
	FArcMaterialCraftContext MakeContext(
		const FGameplayTagContainer& IngredientTags,
		float AverageQuality,
		int32 IngredientCount = 2,
		int32 BaseIngredientCount = 2,
		float ExtraCraftTimeBonus = 0.0f,
		const FGameplayTagContainer& RecipeTags = FGameplayTagContainer())
	{
		FArcMaterialCraftContext Ctx;
		if (IngredientTags.Num() > 0)
		{
			Ctx.PerSlotTags.Add(IngredientTags);
		}
		Ctx.RecipeTags = RecipeTags;
		Ctx.AverageQuality = AverageQuality;
		Ctx.BandEligibilityQuality = AverageQuality;
		Ctx.IngredientCount = IngredientCount;
		Ctx.BaseIngredientCount = BaseIngredientCount;
		Ctx.ExtraCraftTimeBonus = ExtraCraftTimeBonus;
		return Ctx;
	}
}

// ===================================================================
// ComputeQualityAndWeightBonus tests
// ===================================================================

TEST_CLASS(MaterialCraft_QualityAndWeightBonus, "ArcCraft.MaterialCraft.QualityAndWeightBonus")
{
	TEST_METHOD(BaseQuality_NoRemedies_QualityUnchanged_ZeroBonus)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.1f;
		Table->ExtraTimeWeightBonusCap = 1.0f;

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.5f, /*IngredientCount=*/ 3, /*BaseIngredientCount=*/ 3);

		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.5f, Ctx.BandEligibilityQuality, 0.001f, TEXT("BandEligibilityQuality should equal AverageQuality when no remedies")));
		ASSERT_THAT(IsNear(0.0f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus should be 0 when no remedies")));
	}

	TEST_METHOD(ExtraIngredients_AddsWeightBonus_NotQuality)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.1f;
		Table->ExtraTimeWeightBonusCap = 1.0f;

		// 5 ingredients, base is 3 => 2 extra => weight bonus +0.2
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.0f, /*IngredientCount=*/ 5, /*BaseIngredientCount=*/ 3);

		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.0f, Ctx.BandEligibilityQuality, 0.001f, TEXT("BandEligibilityQuality should equal AverageQuality (extras do not boost it)")));
		ASSERT_THAT(IsNear(0.2f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus = 2 * 0.1 = 0.2")));
	}

	TEST_METHOD(FewerIngredientsThanBase_NoNegativeBonus)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.5f;

		// 1 ingredient, base is 3 => max(0, 1-3) = 0 extra
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 2.0f, /*IngredientCount=*/ 1, /*BaseIngredientCount=*/ 3);

		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(2.0f, Ctx.BandEligibilityQuality, 0.001f, TEXT("BandEligibilityQuality should equal AverageQuality")));
		ASSERT_THAT(IsNear(0.0f, Ctx.BandWeightBonus, 0.001f, TEXT("Should not subtract for fewer ingredients")));
	}

	TEST_METHOD(ExtraCraftTimeBonus_AddsCappedWeightBonus)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.0f;
		Table->ExtraTimeWeightBonusCap = 0.5f;

		// Time bonus 0.8, cap 0.5 => only 0.5 added to weight bonus
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.0f, 2, 2, /*ExtraCraftTimeBonus=*/ 0.8f);

		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.0f, Ctx.BandEligibilityQuality, 0.001f, TEXT("BandEligibilityQuality should equal AverageQuality (extras do not boost it)")));
		ASSERT_THAT(IsNear(0.5f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus = min(0.8, 0.5) = 0.5")));
	}

	TEST_METHOD(ExtraCraftTimeBonus_BelowCap)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.0f;
		Table->ExtraTimeWeightBonusCap = 2.0f;

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.0f, 2, 2, /*ExtraCraftTimeBonus=*/ 0.3f);

		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.0f, Ctx.BandEligibilityQuality, 0.001f, TEXT("BandEligibilityQuality should equal AverageQuality")));
		ASSERT_THAT(IsNear(0.3f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus = 0.3 (below cap)")));
	}

	TEST_METHOD(BothRemedies_CombinedWeightBonus)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.2f;
		Table->ExtraTimeWeightBonusCap = 1.0f;

		// 4 ingredients, base 2 => 2 extra => +0.4; time bonus 0.5 => total weight bonus +0.9
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.0f, /*IngredientCount=*/ 4, /*BaseIngredientCount=*/ 2, /*ExtraCraftTimeBonus=*/ 0.5f);

		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.0f, Ctx.BandEligibilityQuality, 0.001f, TEXT("BandEligibilityQuality should equal AverageQuality (extras do not boost it)")));
		ASSERT_THAT(IsNear(0.9f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus = 2*0.2 + 0.5 = 0.9")));
	}

	TEST_METHOD(NullTable_QualityUnchanged_ZeroBonus)
	{
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 2.5f, 5, 2, 0.5f);

		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(nullptr, Ctx);
		ASSERT_THAT(IsNear(2.5f, Ctx.BandEligibilityQuality, 0.001f, TEXT("Null table should leave BandEligibilityQuality as AverageQuality")));
		ASSERT_THAT(IsNear(0.0f, Ctx.BandWeightBonus, 0.001f, TEXT("Null table should leave BandWeightBonus as 0")));
	}
};

// ===================================================================
// Rule Matching tests (via EvaluateRules with single rule)
// ===================================================================

TEST_CLASS(MaterialCraft_RuleMatching, "ArcCraft.MaterialCraft.RuleMatching")
{
	TEST_METHOD(MatchAnyTag_SingleTag_Matches)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Metal.Iron should match rule matching any Resource.Metal")));
	}

	TEST_METHOD(MatchAnyTag_NoMatch_Empty)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Wood);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Wood should not match Metal rule")));
	}

	TEST_METHOD(MatchAllTags_ComboRule_MatchesBothPresent)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer ComboTags;
		ComboTags.AddTag(TAG_Test_Resource_Metal);
		ComboTags.AddTag(TAG_Test_Resource_Gem);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAll(
			FText::FromString("Metal+Gem Combo"), ComboTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);
		IngredientTags.AddTag(TAG_Test_Resource_Gem);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Metal.Iron + Gem should match Metal AND Gem combo rule")));
	}

	TEST_METHOD(MatchAllTags_ComboRule_OnlyOnePresent_NoMatch)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer ComboTags;
		ComboTags.AddTag(TAG_Test_Resource_Metal);
		ComboTags.AddTag(TAG_Test_Resource_Gem);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAll(
			FText::FromString("Metal+Gem Combo"), ComboTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		// Only metal, no gem
		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Only Metal should not match Metal AND Gem combo rule")));
	}

	TEST_METHOD(RequiredRecipeTags_Filters)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal for Weapons"), MetalTags);
		Rule.RequiredRecipeTags.AddTag(TAG_Test_Recipe_Weapon);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		// Recipe tagged as Armor, not Weapon
		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_Test_Recipe_Armor);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f, 2, 2, 0.0f, RecipeTags);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Armor recipe should not match rule requiring Weapon tag")));
	}

	TEST_METHOD(RequiredRecipeTags_Matches)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal for Weapons"), MetalTags);
		Rule.RequiredRecipeTags.AddTag(TAG_Test_Recipe_Weapon);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_Test_Recipe_Weapon);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f, 2, 2, 0.0f, RecipeTags);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Weapon recipe should match rule requiring Weapon tag")));
	}

	TEST_METHOD(EmptyTagQuery_NoMatch)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Empty Query Rule");
		// TagQuery is empty (default)
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Empty tag query should never match")));
	}
};

// ===================================================================
// Band Selection tests (via EvaluateRules)
// ===================================================================

TEST_CLASS(MaterialCraft_BandSelection, "ArcCraft.MaterialCraft.BandSelection")
{
	TEST_METHOD(LowQuality_OnlyFirstBandEligible)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 10.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Uncommon"), 2.0f, 5.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Rare"), 3.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 0.5f);
		Ctx.BandEligibilityQuality = 0.5f;

		// Run many evaluations to verify only Common is selected
		for (int32 i = 0; i < 50; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num()));
			ASSERT_THAT(AreEqual(0, Evals[0].SelectedBandIndex, TEXT("Low quality (0.5) should only select band 0 (Common)")));
		}
	}

	TEST_METHOD(HighQuality_CanSelectAnyBand)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Uncommon"), 1.0f, 1.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Rare"), 2.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 5.0f);
		Ctx.BandEligibilityQuality = 5.0f;

		// All three bands should be eligible at quality 5.0
		TSet<int32> SelectedBands;
		for (int32 i = 0; i < 200; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			if (Evals.Num() > 0)
			{
				SelectedBands.Add(Evals[0].SelectedBandIndex);
			}
		}
		// With equal weights and quality well above all thresholds, all bands should appear
		ASSERT_THAT(IsTrue(SelectedBands.Contains(0), TEXT("Common band should be selectable")));
		ASSERT_THAT(IsTrue(SelectedBands.Contains(1), TEXT("Uncommon band should be selectable")));
		ASSERT_THAT(IsTrue(SelectedBands.Contains(2), TEXT("Rare band should be selectable")));
	}

	TEST_METHOD(NoBandsOnRule_NoEvaluation)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal No Bands"), MetalTags);
		// No bands added
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Rule with no bands should produce no evaluations")));
	}

	TEST_METHOD(QualityBelowAllBands_NoEvaluation)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal High Bands"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Epic"), 5.0f, 1.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Legendary"), 10.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Quality 1.0 should not reach bands starting at 5.0")));
	}

	TEST_METHOD(WeightBonusDoesNotUnlockNewBands)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 1.0f;
		Table->ExtraTimeWeightBonusCap = 10.0f;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Epic"), 5.0f, 100.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		// AverageQuality=1.0, lots of extra ingredients and time => large BandWeightBonus
		// But BandEligibilityQuality stays at 1.0, so Epic band (MinQuality=5.0) should NOT be reachable
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f, /*IngredientCount=*/ 10, /*BaseIngredientCount=*/ 2, /*ExtraCraftTimeBonus=*/ 5.0f);
		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);

		for (int32 i = 0; i < 50; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num()));
			ASSERT_THAT(AreEqual(0, Evals[0].SelectedBandIndex, TEXT("Weight bonus should not unlock Epic band (MinQuality=5.0) when BandEligibilityQuality=1.0")));
		}
	}
};

// ===================================================================
// Priority and MaxActiveRules tests
// ===================================================================

TEST_CLASS(MaterialCraft_PriorityAndLimits, "ArcCraft.MaterialCraft.PriorityAndLimits")
{
	TEST_METHOD(MaxActiveRules_LimitsMatchedRules)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->MaxActiveRules = 2;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		// Add 3 rules that all match "Metal", with different priorities
		for (int32 i = 0; i < 3; ++i)
		{
			FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
				FText::FromString(FString::Printf(TEXT("Rule_%d"), i)), MetalTags, /*Priority=*/ i);
			Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
			Table->Rules.Add(Rule);
		}

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(2, Evals.Num(), TEXT("MaxActiveRules=2 should limit to 2 evaluations")));
	}

	TEST_METHOD(HigherPriority_SelectedFirst)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->MaxActiveRules = 1;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		// Low priority rule
		FArcMaterialPropertyRule LowRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Low Priority"), MetalTags, /*Priority=*/ 1);
		LowRule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(LowRule);

		// High priority rule
		FArcMaterialPropertyRule HighRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("High Priority"), MetalTags, /*Priority=*/ 10);
		HighRule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(HighRule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num()));
		// High priority rule is at index 1 in the table
		ASSERT_THAT(AreEqual(1, Evals[0].RuleIndex, TEXT("Higher priority rule (index 1) should be selected")));
	}

	TEST_METHOD(MaxContributions_MultipleFromSameRule)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal Multi"), MetalTags);
		Rule.MaxContributions = 3;
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(3, Evals.Num(), TEXT("MaxContributions=3 should produce 3 evaluations from one rule")));
	}

	TEST_METHOD(MultipleRules_BothMatch)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		// Rule 1: Metal
		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);
		FArcMaterialPropertyRule MetalRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		MetalRule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(MetalRule);

		// Rule 2: Gem
		FGameplayTagContainer GemTags;
		GemTags.AddTag(TAG_Test_Resource_Gem);
		FArcMaterialPropertyRule GemRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Gem"), GemTags);
		GemRule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(GemRule);

		// Ingredients have both Metal and Gem
		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);
		IngredientTags.AddTag(TAG_Test_Resource_Gem);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(2, Evals.Num(), TEXT("Both Metal and Gem rules should match")));
	}
};

// ===================================================================
// Band Budget tests
// ===================================================================

TEST_CLASS(MaterialCraft_BandBudget, "ArcCraft.MaterialCraft.BandBudget")
{
	TEST_METHOD(BudgetEnabled_LimitsHighBands)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->BaseBandBudget = 1.0f; // Only enough for band index 0 (cost=1)
		Table->BudgetPerQuality = 0.0f;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Uncommon"), 0.0f, 100.0f)); // Very high weight but costs 2
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		// With budget 1.0, first pick might be band 1 (cost=2, over budget) => falls back to band 0 (cost=1)
		for (int32 i = 0; i < 50; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num()));
			ASSERT_THAT(AreEqual(0, Evals[0].SelectedBandIndex,
				TEXT("Budget=1 should only afford band 0 (cost=1)")));
		}
	}

	TEST_METHOD(BudgetPerQuality_IncreasesWithQuality)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->BaseBandBudget = 1.0f;
		Table->BudgetPerQuality = 2.0f;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		// Band 0: cost=1, Band 1: cost=2
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Uncommon"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		// BandEligibilityQuality = 2.0 => budget = 1.0 + max(0, 2.0-1.0)*2.0 = 3.0 => can afford band 1 (cost=2)
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 2.0f);
		Ctx.BandEligibilityQuality = 2.0f;

		TSet<int32> SelectedBands;
		for (int32 i = 0; i < 100; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			if (Evals.Num() > 0)
			{
				SelectedBands.Add(Evals[0].SelectedBandIndex);
			}
		}
		ASSERT_THAT(IsTrue(SelectedBands.Contains(1), TEXT("With sufficient budget, Uncommon band should be selectable")));
	}

	TEST_METHOD(NoBudget_AllBandsAvailable)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		Table->BaseBandBudget = 0.0f; // Budget disabled
		Table->BudgetPerQuality = 0.0f;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Rare"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TSet<int32> SelectedBands;
		for (int32 i = 0; i < 100; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			if (Evals.Num() > 0)
			{
				SelectedBands.Add(Evals[0].SelectedBandIndex);
			}
		}
		ASSERT_THAT(IsTrue(SelectedBands.Contains(0), TEXT("Common should be selectable")));
		ASSERT_THAT(IsTrue(SelectedBands.Contains(1), TEXT("Rare should be selectable without budget")));
	}
};

// ===================================================================
// Edge cases and null safety
// ===================================================================

TEST_CLASS(MaterialCraft_EdgeCases, "ArcCraft.MaterialCraft.EdgeCases")
{
	TEST_METHOD(NullTable_ReturnsEmpty)
	{
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(FGameplayTagContainer(), 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(nullptr, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Null table should return empty evaluations")));
	}

	TEST_METHOD(EmptyRules_ReturnsEmpty)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();
		// No rules added

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(FGameplayTagContainer(), 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Empty rules should return empty evaluations")));
	}

	TEST_METHOD(EmptyIngredientTags_NoRuleMatches)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		// Empty ingredient tags
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(FGameplayTagContainer(), 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Empty ingredient tags should not match any rule")));
	}

	TEST_METHOD(EvaluationStoresCorrectRule)
	{
		UArcMaterialPropertyTable* Table = ArcCraftTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.QualityBands.Add(ArcCraftTestHelpers::MakeBand(FText::FromString("Common"), 0.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num()));
		ASSERT_THAT(AreEqual(0, Evals[0].RuleIndex, TEXT("Should reference rule index 0")));
		ASSERT_THAT(IsNotNull(Evals[0].Rule, TEXT("Rule pointer should be set")));
		ASSERT_THAT(IsNotNull(Evals[0].Band, TEXT("Band pointer should be set")));
		ASSERT_THAT(IsNear(1.0f, Evals[0].BandEligibilityQuality, 0.001f, TEXT("BandEligibilityQuality should be stored")));
	}
};
