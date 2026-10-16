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
#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableData.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"

// ---- Test tags ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Metal, "Test.Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Metal_Iron, "Test.Resource.Metal.Iron");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Metal_Steel, "Test.Resource.Metal.Steel");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Gem, "Test.Resource.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Resource_Wood, "Test.Resource.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Recipe_Weapon, "Test.Recipe.Weapon");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Recipe_Armor, "Test.Recipe.Armor");

// ===================================================================
// Helper: Build a property table in memory for tests
// ===================================================================

namespace ArcCraftTestHelpers
{
	/** Create a simple stat modifier FInstancedStruct with a given R gate. */
	FInstancedStruct MakeStatModifier(float MinR = 0.0f)
	{
		FArcCraftModifier_Stats StatMod;
		StatMod.StatId = TEXT("test_stat");
		StatMod.MinR = MinR;

		FInstancedStruct Instance;
		Instance.InitializeAs<FArcCraftModifier_Stats>(StatMod);
		return Instance;
	}

	/** Create a rule that matches any of the given tags. */
	FArcMaterialPropertyRule MakeRuleMatchAny(const FText& Name, const FGameplayTagContainer& Tags)
	{
		FArcMaterialPropertyRule Rule;
		Rule.RuleName = Name;
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(Tags);
		return Rule;
	}

	/** Create a rule that requires ALL of the given tags (AND match). */
	FArcMaterialPropertyRule MakeRuleMatchAll(const FText& Name, const FGameplayTagContainer& Tags)
	{
		FArcMaterialPropertyRule Rule;
		Rule.RuleName = Name;
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(Tags);
		return Rule;
	}

	/** Build a minimal context with tags and R, no items involved.
	 *  IngredientTags are placed into a single PerSlotTags entry (simulating one slot). */
	FArcMaterialCraftContext MakeContext(
		const FGameplayTagContainer& IngredientTags,
		float AverageR,
		int32 IngredientCount = 2,
		int32 BaseIngredientCount = 2,
		float ExtraCraftTimeBonus = 0.0f)
	{
		FArcMaterialCraftContext Ctx;
		if (IngredientTags.Num() > 0)
		{
			Ctx.PerSlotTags.Add(IngredientTags);
		}
		Ctx.AverageR = AverageR;
		Ctx.BandEligibilityR = AverageR;
		Ctx.IngredientCount = IngredientCount;
		Ctx.BaseIngredientCount = BaseIngredientCount;
		Ctx.ExtraCraftTimeBonus = ExtraCraftTimeBonus;
		return Ctx;
	}
}

// ===================================================================
// ComputeEligibilityRAndWeightBonus tests
// ===================================================================

TEST_CLASS(MaterialCraft_RAndWeightBonus, "ArcCraft.MaterialCraft.RAndWeightBonus")
{
	TEST_METHOD(BaseR_NoRemedies_RUnchanged_ZeroBonus)
	{
		FArcMaterialPropertyTableData Table;
		Table.ExtraIngredientWeightBonus = 0.1f;
		Table.ExtraTimeWeightBonusCap = 1.0f;

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.5f, /*IngredientCount=*/ 3, /*BaseIngredientCount=*/ 3);

		FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.5f, Ctx.BandEligibilityR, 0.001f, TEXT("BandEligibilityR should equal AverageR when no remedies")));
		ASSERT_THAT(IsNear(0.0f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus should be 0 when no remedies")));
	}

	TEST_METHOD(ExtraIngredients_AddsWeightBonus_NotR)
	{
		FArcMaterialPropertyTableData Table;
		Table.ExtraIngredientWeightBonus = 0.1f;
		Table.ExtraTimeWeightBonusCap = 1.0f;

		// 5 ingredients, base is 3 => 2 extra => weight bonus +0.2
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.0f, /*IngredientCount=*/ 5, /*BaseIngredientCount=*/ 3);

		FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.0f, Ctx.BandEligibilityR, 0.001f, TEXT("BandEligibilityR should equal AverageR (extras do not boost it)")));
		ASSERT_THAT(IsNear(0.2f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus = 2 * 0.1 = 0.2")));
	}

	TEST_METHOD(FewerIngredientsThanBase_NoNegativeBonus)
	{
		FArcMaterialPropertyTableData Table;
		Table.ExtraIngredientWeightBonus = 0.5f;

		// 1 ingredient, base is 3 => max(0, 1-3) = 0 extra
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 2.0f, /*IngredientCount=*/ 1, /*BaseIngredientCount=*/ 3);

		FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(2.0f, Ctx.BandEligibilityR, 0.001f, TEXT("BandEligibilityR should equal AverageR")));
		ASSERT_THAT(IsNear(0.0f, Ctx.BandWeightBonus, 0.001f, TEXT("Should not subtract for fewer ingredients")));
	}

	TEST_METHOD(ExtraCraftTimeBonus_AddsCappedWeightBonus)
	{
		FArcMaterialPropertyTableData Table;
		Table.ExtraIngredientWeightBonus = 0.0f;
		Table.ExtraTimeWeightBonusCap = 0.5f;

		// Time bonus 0.8, cap 0.5 => only 0.5 added to weight bonus
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.0f, 2, 2, /*ExtraCraftTimeBonus=*/ 0.8f);

		FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.0f, Ctx.BandEligibilityR, 0.001f, TEXT("BandEligibilityR should equal AverageR (extras do not boost it)")));
		ASSERT_THAT(IsNear(0.5f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus = min(0.8, 0.5) = 0.5")));
	}

	TEST_METHOD(ExtraCraftTimeBonus_BelowCap)
	{
		FArcMaterialPropertyTableData Table;
		Table.ExtraIngredientWeightBonus = 0.0f;
		Table.ExtraTimeWeightBonusCap = 2.0f;

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.0f, 2, 2, /*ExtraCraftTimeBonus=*/ 0.3f);

		FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.0f, Ctx.BandEligibilityR, 0.001f, TEXT("BandEligibilityR should equal AverageR")));
		ASSERT_THAT(IsNear(0.3f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus = 0.3 (below cap)")));
	}

	TEST_METHOD(BothRemedies_CombinedWeightBonus)
	{
		FArcMaterialPropertyTableData Table;
		Table.ExtraIngredientWeightBonus = 0.2f;
		Table.ExtraTimeWeightBonusCap = 1.0f;

		// 4 ingredients, base 2 => 2 extra => +0.4; time bonus 0.5 => total weight bonus +0.9
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(
			FGameplayTagContainer(), 1.0f, /*IngredientCount=*/ 4, /*BaseIngredientCount=*/ 2, /*ExtraCraftTimeBonus=*/ 0.5f);

		FArcMaterialCraftEvaluator::ComputeEligibilityRAndWeightBonus(Table, Ctx);
		ASSERT_THAT(IsNear(1.0f, Ctx.BandEligibilityR, 0.001f, TEXT("BandEligibilityR should equal AverageR (extras do not boost it)")));
		ASSERT_THAT(IsNear(0.9f, Ctx.BandWeightBonus, 0.001f, TEXT("BandWeightBonus = 2*0.2 + 0.5 = 0.9")));
	}
};

// ===================================================================
// Rule Matching tests (via EvaluateRules with single rule)
// ===================================================================

TEST_CLASS(MaterialCraft_RuleMatching, "ArcCraft.MaterialCraft.RuleMatching")
{
	TEST_METHOD(MatchAnyTag_SingleTag_Matches)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Metal.Iron should match rule matching any Resource.Metal")));
	}

	TEST_METHOD(MatchAnyTag_NoMatch_Empty)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Wood);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Wood should not match Metal rule")));
	}

	TEST_METHOD(MatchAllTags_ComboRule_MatchesBothPresent)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer ComboTags;
		ComboTags.AddTag(TAG_Test_Resource_Metal);
		ComboTags.AddTag(TAG_Test_Resource_Gem);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAll(
			FText::FromString("Metal+Gem Combo"), ComboTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		// Metal in slot A, Gem in slot B: only the union across slots satisfies the AND query.
		FGameplayTagContainer MetalSlotTags;
		MetalSlotTags.AddTag(TAG_Test_Resource_Metal_Iron);
		FGameplayTagContainer GemSlotTags;
		GemSlotTags.AddTag(TAG_Test_Resource_Gem);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(FGameplayTagContainer(), 1.0f);
		Ctx.PerSlotTags.Add(MetalSlotTags);
		Ctx.PerSlotTags.Add(GemSlotTags);
		ASSERT_THAT(AreEqual(2, Ctx.PerSlotTags.Num(), TEXT("Metal and Gem must sit in two separate slot entries")));

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Metal.Iron in one slot + Gem in another should match Metal AND Gem combo rule through the cross-slot union")));
	}

	TEST_METHOD(MatchAllTags_ComboRule_OnlyOnePresent_NoMatch)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer ComboTags;
		ComboTags.AddTag(TAG_Test_Resource_Metal);
		ComboTags.AddTag(TAG_Test_Resource_Gem);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAll(
			FText::FromString("Metal+Gem Combo"), ComboTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		// Only metal, no gem
		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Only Metal should not match Metal AND Gem combo rule")));
	}

	TEST_METHOD(EmptyTagQuery_NoMatch)
	{
		FArcMaterialPropertyTableData Table;

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Empty Query Rule");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_ExactMatchAnyTags(FGameplayTagContainer{TAG_Test_Resource_Gem});
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Rule with non-matching tag query should not fire")));
	}
};

// ===================================================================
// Rule-level MinAverageR gate tests
// ===================================================================

TEST_CLASS(MaterialCraft_RuleMinAverageR, "ArcCraft.MaterialCraft.RuleMinAverageR")
{
	TEST_METHOD(Rule_MinAverageR_Gates_WhenRBelow)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal High"), MetalTags);
		Rule.MinAverageR = 0.5f;
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		// R 0.3 < MinAverageR 0.5 — rule should NOT fire
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 0.3f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Rule with MinAverageR=0.5 should NOT fire at AverageR=0.3")));
	}

	TEST_METHOD(Rule_MinAverageR_Passes_WhenRAtThreshold)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal High"), MetalTags);
		Rule.MinAverageR = 0.5f;
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		// R exactly at threshold — should fire
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 0.5f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Rule with MinAverageR=0.5 should fire at AverageR=0.5")));
	}

	TEST_METHOD(Rule_MinAverageR_DefaultZero_AlwaysFires)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal Default"), MetalTags);
		// MinAverageR = 0.0f (default) — should always fire if tag matches
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 0.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Rule with default MinAverageR=0 should fire at any R")));
	}

	TEST_METHOD(TwoRules_SameTagQuery_DifferentMinAverageR_OnlyBaseFires)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		// Rule 0: fires at any R (MinAverageR=0)
		FArcMaterialPropertyRule LowRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal Base"), MetalTags);
		LowRule.MinAverageR = 0.0f;
		LowRule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(LowRule);

		// Rule 1: fires only at Q>=0.8
		FArcMaterialPropertyRule HighRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal Bonus"), MetalTags);
		HighRule.MinAverageR = 0.8f;
		HighRule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(HighRule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		// R 0.5 — only the base rule should fire
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 0.5f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Only the base rule (MinAverageR=0) should fire at Q=0.5")));
		ASSERT_THAT(AreEqual(0, Evals[0].RuleIndex, TEXT("Should be rule 0 (base rule)")));
	}

	TEST_METHOD(TwoRules_SameTagQuery_DifferentMinAverageR_BothFireAtHighR)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule LowRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal Base"), MetalTags);
		LowRule.MinAverageR = 0.0f;
		LowRule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(LowRule);

		FArcMaterialPropertyRule HighRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal Bonus"), MetalTags);
		HighRule.MinAverageR = 0.8f;
		HighRule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(HighRule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		// R 1.0 — both rules should fire
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(2, Evals.Num(), TEXT("Both rules should fire at Q=1.0")));
	}
};

// ===================================================================
// All-eligible-fire modifier tests
// ===================================================================

TEST_CLASS(MaterialCraft_ModifierEligibility, "ArcCraft.MaterialCraft.ModifierEligibility")
{
	TEST_METHOD(RuleModifiers_AllEligible_Fire)
	{
		// Multiple modifiers on a rule with no threshold — all should be in EligibleModifierIndices
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		// Three modifiers, all with MinR=0
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Rule should produce one evaluation")));
		ASSERT_THAT(AreEqual(3, Evals[0].EligibleModifierIndices.Num(),
			TEXT("All 3 modifiers should be eligible (MinR=0)")));
		ASSERT_THAT(AreEqual(0, Evals[0].EligibleModifierIndices[0]));
		ASSERT_THAT(AreEqual(1, Evals[0].EligibleModifierIndices[1]));
		ASSERT_THAT(AreEqual(2, Evals[0].EligibleModifierIndices[2]));
	}

	TEST_METHOD(RuleModifiers_RThreshold_Gates)
	{
		// One modifier cleared, one not — only cleared modifier is eligible
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		// Modifier 0: always eligible (threshold=0)
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		// Modifier 1: requires Q>=0.9 — NOT met at Q=0.5
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier(0.9f));
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 0.5f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num()));
		ASSERT_THAT(AreEqual(1, Evals[0].EligibleModifierIndices.Num(),
			TEXT("Only modifier 0 should be eligible at Q=0.5 (modifier 1 needs Q>=0.9)")));
		ASSERT_THAT(AreEqual(0, Evals[0].EligibleModifierIndices[0],
			TEXT("Index 0 should be the eligible modifier")));
	}

	TEST_METHOD(RuleModifiers_AllThresholdsMet_AllEligible)
	{
		// Both modifiers have thresholds that are met — both should fire
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier(0.3f));
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier(0.8f));
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		// Q=1.0 clears both thresholds (0.3 and 0.8)
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num()));
		ASSERT_THAT(AreEqual(2, Evals[0].EligibleModifierIndices.Num(),
			TEXT("Both modifiers should be eligible at Q=1.0")));
	}

	TEST_METHOD(RuleModifiers_AllThresholdsExceed_ZeroEligible_NoEvaluation)
	{
		// All modifiers have thresholds that exceed R — no evaluation produced
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier(5.0f));
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier(10.0f));
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		// Rule matches tag query, but zero modifiers are eligible -> no evaluation
		ASSERT_THAT(AreEqual(0, Evals.Num(),
			TEXT("No evaluation should be produced when all modifier thresholds exceed R")));
	}

	TEST_METHOD(NoModifiersOnRule_NoEvaluation)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal No Modifiers"), MetalTags);
		// No modifiers added
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Rule with no modifiers should produce no evaluations")));
	}
};

// ===================================================================
// Rule count tests
// ===================================================================

TEST_CLASS(MaterialCraft_RuleCount, "ArcCraft.MaterialCraft.RuleCount")
{
	TEST_METHOD(SingleRule_ProducesOneEvaluation)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal Single"), MetalTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("One matched rule should produce exactly one evaluation")));
	}

	TEST_METHOD(MultipleRules_BothMatch)
	{
		FArcMaterialPropertyTableData Table;

		// Rule 1: Metal
		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);
		FArcMaterialPropertyRule MetalRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		MetalRule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(MetalRule);

		// Rule 2: Gem
		FGameplayTagContainer GemTags;
		GemTags.AddTag(TAG_Test_Resource_Gem);
		FArcMaterialPropertyRule GemRule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Gem"), GemTags);
		GemRule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(GemRule);

		// Ingredients have both Metal and Gem
		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);
		IngredientTags.AddTag(TAG_Test_Resource_Gem);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(2, Evals.Num(), TEXT("Both Metal and Gem rules should match")));
	}
};

// ===================================================================
// Edge cases and null safety
// ===================================================================

TEST_CLASS(MaterialCraft_EdgeCases, "ArcCraft.MaterialCraft.EdgeCases")
{
	TEST_METHOD(EmptyRules_ReturnsEmpty)
	{
		FArcMaterialPropertyTableData Table;
		// No rules added

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(FGameplayTagContainer(), 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Empty rules should return empty evaluations")));
	}

	TEST_METHOD(EmptyIngredientTags_NoRuleMatches)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		// Empty ingredient tags
		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(FGameplayTagContainer(), 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Empty ingredient tags should not match any rule")));
	}

	TEST_METHOD(EvaluationStoresCorrectRule)
	{
		FArcMaterialPropertyTableData Table;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialPropertyRule Rule = ArcCraftTestHelpers::MakeRuleMatchAny(
			FText::FromString("Metal"), MetalTags);
		Rule.Modifiers.Add(ArcCraftTestHelpers::MakeStatModifier());
		Table.Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_Test_Resource_Metal);

		FArcMaterialCraftContext Ctx = ArcCraftTestHelpers::MakeContext(IngredientTags, 1.0f);

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num()));
		ASSERT_THAT(AreEqual(0, Evals[0].RuleIndex, TEXT("Should reference rule index 0")));
		ASSERT_THAT(IsNotNull(Evals[0].Rule, TEXT("Rule pointer should be set")));
		ASSERT_THAT(AreEqual(1, Evals[0].EligibleModifierIndices.Num(), TEXT("One modifier should be eligible")));
	}
};
