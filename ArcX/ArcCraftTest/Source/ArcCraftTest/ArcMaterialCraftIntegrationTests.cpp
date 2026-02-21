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
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "ArcCraft/MaterialCraft/ArcMaterialOutputModifier.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "ArcCraft/Recipe/ArcRecipeQuality.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcCraftIntegrationTest, Log, All);

// ---- Test tags (unique prefix for this TU) ----
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Resource_Metal, "Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Resource_Metal_Iron, "Resource.Metal.Iron");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Resource_Gem, "Resource.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Resource_Gem_Ruby, "Resource.Gem.Ruby");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Resource_Wood, "Resource.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Recipe_Weapon, "Recipe.Weapon");

// ===================================================================
// Helpers: create bands with actual stat modifiers inside
// ===================================================================

namespace IntegrationTestHelpers
{
	/** Create a transient property table. */
	UArcMaterialPropertyTable* CreateTransientTable()
	{
		return NewObject<UArcMaterialPropertyTable>(GetTransientPackage());
	}

	/**
	 * Create a stat modifier with the given base value and scaling factor.
	 * Returns an FInstancedStruct wrapping FArcRecipeOutputModifier_Stats.
	 */
	FInstancedStruct MakeStatModifier(float BaseValue, float QualityScalingFactor = 1.0f)
	{
		FArcRecipeOutputModifier_Stats StatMod;
		FArcItemAttributeStat Stat;
		Stat.SetValue(BaseValue);
		StatMod.BaseStats.Add(Stat);
		StatMod.QualityScalingFactor = QualityScalingFactor;

		FInstancedStruct Instance;
		Instance.InitializeAs<FArcRecipeOutputModifier_Stats>(StatMod);
		return Instance;
	}

	/**
	 * Create a stat modifier with multiple stats.
	 */
	FInstancedStruct MakeMultiStatModifier(const TArray<float>& BaseValues, float QualityScalingFactor = 1.0f)
	{
		FArcRecipeOutputModifier_Stats StatMod;
		StatMod.QualityScalingFactor = QualityScalingFactor;
		for (const float Value : BaseValues)
		{
			FArcItemAttributeStat Stat;
			Stat.SetValue(Value);
			StatMod.BaseStats.Add(Stat);
		}

		FInstancedStruct Instance;
		Instance.InitializeAs<FArcRecipeOutputModifier_Stats>(StatMod);
		return Instance;
	}

	/** Create a quality band with stat modifiers embedded. */
	FArcMaterialQualityBand MakeBandWithStats(
		const FText& Name,
		float MinQuality,
		float BaseWeight,
		float QualityWeightBias,
		const TArray<FInstancedStruct>& Modifiers)
	{
		FArcMaterialQualityBand Band;
		Band.BandName = Name;
		Band.MinQuality = MinQuality;
		Band.BaseWeight = BaseWeight;
		Band.QualityWeightBias = QualityWeightBias;
		Band.Modifiers = Modifiers;
		return Band;
	}

	/** Create a simple quality band with one stat modifier. */
	FArcMaterialQualityBand MakeBandWithSingleStat(
		const FText& Name,
		float MinQuality,
		float BaseWeight,
		float QualityWeightBias,
		float StatBaseValue,
		float QualityScalingFactor = 1.0f)
	{
		TArray<FInstancedStruct> Modifiers;
		Modifiers.Add(MakeStatModifier(StatBaseValue, QualityScalingFactor));
		return MakeBandWithStats(Name, MinQuality, BaseWeight, QualityWeightBias, Modifiers);
	}

	/** Build a minimal FArcMaterialCraftContext. */
	FArcMaterialCraftContext MakeContext(
		const FGameplayTagContainer& IngredientTags,
		float AverageQuality,
		int32 IngredientCount = 2,
		int32 BaseIngredientCount = 2,
		float ExtraCraftTimeBonus = 0.0f,
		const FGameplayTagContainer& RecipeTags = FGameplayTagContainer())
	{
		FArcMaterialCraftContext Ctx;
		Ctx.PerSlotTags = { IngredientTags };
		Ctx.RecipeTags = RecipeTags;
		Ctx.AverageQuality = AverageQuality;
		Ctx.BandEligibilityQuality = AverageQuality;
		Ctx.IngredientCount = IngredientCount;
		Ctx.BaseIngredientCount = BaseIngredientCount;
		Ctx.ExtraCraftTimeBonus = ExtraCraftTimeBonus;
		return Ctx;
	}

	/** Log the evaluations and resulting item spec for debugging. */
	void LogEvaluationResults(
		const TArray<FArcMaterialRuleEvaluation>& Evaluations,
		const FArcItemSpec& OutSpec,
		const TCHAR* TestLabel)
	{
		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("=== %s: %d evaluations ==="), TestLabel, Evaluations.Num());

		for (int32 Idx = 0; Idx < Evaluations.Num(); ++Idx)
		{
			const FArcMaterialRuleEvaluation& Eval = Evaluations[Idx];
			UE_LOG(LogArcCraftIntegrationTest, Log,
				TEXT("  [%d] Rule=%d '%s' | Band=%d '%s' | BandEligQ=%.2f | EffWeight=%.2f"),
				Idx,
				Eval.RuleIndex,
				Eval.Rule ? *Eval.Rule->RuleName.ToString() : TEXT("null"),
				Eval.SelectedBandIndex,
				Eval.Band ? *Eval.Band->BandName.ToString() : TEXT("null"),
				Eval.BandEligibilityQuality,
				Eval.EffectiveWeight);

			if (Eval.Band)
			{
				UE_LOG(LogArcCraftIntegrationTest, Log,
					TEXT("       Band has %d modifier(s)"), Eval.Band->Modifiers.Num());
			}
		}

		// Log output item stats
		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		if (StatsFragment)
		{
			UE_LOG(LogArcCraftIntegrationTest, Log,
				TEXT("  Output has %d stat(s):"), StatsFragment->DefaultStats.Num());
			for (int32 i = 0; i < StatsFragment->DefaultStats.Num(); ++i)
			{
				UE_LOG(LogArcCraftIntegrationTest, Log,
					TEXT("    Stat[%d] = %.3f"), i, StatsFragment->DefaultStats[i].Value.GetValue());
			}
		}
		else
		{
			UE_LOG(LogArcCraftIntegrationTest, Log, TEXT("  Output has no stats fragment."));
		}
	}
}

// ===================================================================
// Integration: ApplyEvaluations with real stat modifiers
// ===================================================================

TEST_CLASS(MaterialCraft_ApplyEvaluations, "ArcCraft.MaterialCraft.Integration.ApplyEvaluations")
{
	TEST_METHOD(SingleRule_SingleBand_AppliesStatModifier)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Strength");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;
		// Common band with base stat 10.0
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 10.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal_Iron);

		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Should match one rule")));

		// Apply evaluations to a fresh item spec
		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("SingleRule_SingleBand"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment, TEXT("Should have created stats fragment from band modifier")));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num(), TEXT("Should have one stat from band")));
		ASSERT_THAT(IsNear(10.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Stat should be base value 10.0 at quality 1.0")));
	}

	TEST_METHOD(SingleRule_SingleBand_QualityScalesModifierOutput)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Strength");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 10.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal_Iron);

		// BandEligibilityQuality = 3.0 => QualityScale in stat = 1.0 + (3.0-1.0)*1.0 = 3.0 => stat = 10*3 = 30
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 3.0f);
		Ctx.BandEligibilityQuality = 3.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 3.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("QualityScalesModifier"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(IsNear(30.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Quality 3.0 should triple the base stat (10*3=30)")));
	}

	TEST_METHOD(TwoRules_BothMatch_TwoStatsApplied)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		// Rule 1: Metal => stat 10.0
		{
			FGameplayTagContainer MetalTags;
			MetalTags.AddTag(TAG_IntTest_Resource_Metal);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Metal Strength");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
			Rule.MaxContributions = 1;
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 10.0f, 1.0f));
			Table->Rules.Add(Rule);
		}

		// Rule 2: Gem => stat 25.0
		{
			FGameplayTagContainer GemTags;
			GemTags.AddTag(TAG_IntTest_Resource_Gem);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Gem Brilliance");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(GemTags);
			Rule.MaxContributions = 1;
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 25.0f, 1.0f));
			Table->Rules.Add(Rule);
		}

		// Ingredients have both Metal and Gem
		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		IngredientTags.AddTag(TAG_IntTest_Resource_Gem_Ruby);

		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(2, Evals.Num(), TEXT("Both Metal and Gem rules should match")));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("TwoRules_BothMatch"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment, TEXT("Should have stats from both rules")));
		ASSERT_THAT(AreEqual(2, StatsFragment->DefaultStats.Num(),
			TEXT("Two rules each adding one stat should produce 2 stats total")));

		// Log individual stat values for debugging
		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  TwoRules result: stat[0]=%.3f, stat[1]=%.3f"),
			StatsFragment->DefaultStats[0].Value.GetValue(),
			StatsFragment->DefaultStats[1].Value.GetValue());
	}

	TEST_METHOD(MaxContributions_MultipleStatsFromSameRule)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Multi-Roll");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 3;
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 5.0f, 1.0f));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(3, Evals.Num(), TEXT("MaxContributions=3 should produce 3 evaluations")));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("MaxContributions"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		// 3 contributions * 1 stat each = 3 stats
		ASSERT_THAT(AreEqual(3, StatsFragment->DefaultStats.Num(),
			TEXT("3 contributions should add 3 stats")));
	}

	TEST_METHOD(BandWithMultipleModifiers_AllApplied)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Dual Stat");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;

		// Create a band with TWO stat modifiers
		TArray<FInstancedStruct> Modifiers;
		Modifiers.Add(IntegrationTestHelpers::MakeStatModifier(10.0f, 1.0f)); // Strength
		Modifiers.Add(IntegrationTestHelpers::MakeStatModifier(7.0f, 0.0f));  // Durability (no quality scaling)

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithStats(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, Modifiers));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 2.0f);
		Ctx.BandEligibilityQuality = 2.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num()));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 2.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("BandWithMultipleModifiers"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		// First modifier adds 1 stat, second modifier finds existing fragment and adds 1 more = 2 total
		ASSERT_THAT(AreEqual(2, StatsFragment->DefaultStats.Num(),
			TEXT("Band with 2 modifiers should produce 2 stats")));

		// First stat: 10.0 * (1.0 + (2.0-1.0)*1.0) = 10.0 * 2.0 = 20.0
		ASSERT_THAT(IsNear(20.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("First stat (quality-scaled) should be 20.0")));
		// Second stat: 7.0 * (1.0 + (2.0-1.0)*0.0) = 7.0 * 1.0 = 7.0
		ASSERT_THAT(IsNear(7.0f, StatsFragment->DefaultStats[1].Value.GetValue(), 0.001f,
			TEXT("Second stat (no scaling) should be 7.0")));
	}

	TEST_METHOD(EmptyBandModifiers_NoStatsAdded)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Empty");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;
		// Band with no modifiers
		FArcMaterialQualityBand EmptyBand;
		EmptyBand.BandName = FText::FromString("Empty");
		EmptyBand.MinQuality = 0.0f;
		EmptyBand.BaseWeight = 1.0f;
		// No modifiers added
		Rule.QualityBands.Add(EmptyBand);
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Rule should match even with empty band")));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("EmptyBandModifiers"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNull(StatsFragment, TEXT("Empty band should not create stats fragment")));
	}

	TEST_METHOD(NullBandPointer_NoStatsAdded)
	{
		// Manually construct an evaluation with null Band pointer
		FArcMaterialRuleEvaluation Eval;
		Eval.RuleIndex = 0;
		Eval.SelectedBandIndex = 0;
		Eval.BandEligibilityQuality = 1.0f;
		Eval.EffectiveWeight = 0.0f;
		Eval.Rule = nullptr;
		Eval.Band = nullptr;

		TArray<FArcMaterialRuleEvaluation> Evals = { Eval };

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		// Should not crash
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNull(StatsFragment, TEXT("Null band should not produce stats")));
	}
};

// ===================================================================
// Integration: Multiple bands — verify selection produces correct counts
// ===================================================================

TEST_CLASS(MaterialCraft_BandSelectionWithModifiers, "ArcCraft.MaterialCraft.Integration.BandSelection")
{
	TEST_METHOD(MultipleBands_DifferentStatValues_CountCorrect)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Tiered");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;

		// Band 0: Common stat=5.0  (1 stat modifier)
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 5.0f, 1.0f));
		// Band 1: Uncommon stat=15.0 (1 stat modifier)
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Uncommon"), 1.0f, 1.0f, 0.0f, 15.0f, 1.0f));
		// Band 2: Rare stat=50.0 (1 stat modifier)
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Rare"), 2.0f, 1.0f, 0.0f, 50.0f, 1.0f));

		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		// Run at high quality so all bands are eligible
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 5.0f);
		Ctx.BandEligibilityQuality = 5.0f;

		// Run many times: each time, we should get exactly 1 evaluation, 1 stat
		int32 TotalRuns = 100;
		TSet<int32> ObservedBands;
		TArray<float> ObservedValues;

		for (int32 i = 0; i < TotalRuns; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num()));

			ObservedBands.Add(Evals[0].SelectedBandIndex);

			FArcItemSpec OutSpec;
			TArray<const FArcItemData*> Ingredients;
			TArray<float> QualityMults;

			FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 5.0f);

			const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(StatsFragment, TEXT("Each run should produce stats")));
			ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num(), TEXT("Each run should produce 1 stat")));

			ObservedValues.Add(StatsFragment->DefaultStats[0].Value.GetValue());
		}

		// With equal weights and quality 5.0 well above all thresholds, all bands should appear
		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Common band should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(1), TEXT("Uncommon band should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(2), TEXT("Rare band should appear")));

		// Log distribution for inspection
		int32 CommonCount = 0, UncommonCount = 0, RareCount = 0;
		for (const float V : ObservedValues)
		{
			// QualityScale at BandEligQ 5.0 = 1.0 + (5.0-1.0)*1.0 = 5.0
			// Common:   5.0 * 5.0 = 25.0
			// Uncommon: 15.0 * 5.0 = 75.0
			// Rare:     50.0 * 5.0 = 250.0
			if (FMath::IsNearlyEqual(V, 25.0f, 0.1f)) CommonCount++;
			else if (FMath::IsNearlyEqual(V, 75.0f, 0.1f)) UncommonCount++;
			else if (FMath::IsNearlyEqual(V, 250.0f, 0.1f)) RareCount++;
		}
		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  Band distribution over %d runs: Common=%d, Uncommon=%d, Rare=%d"),
			TotalRuns, CommonCount, UncommonCount, RareCount);
	}

	TEST_METHOD(LowQuality_OnlyCommonBandModifierApplied)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Tiered");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 5.0f, 0.0f)); // zero scaling
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Rare"), 3.0f, 1.0f, 0.0f, 50.0f, 0.0f));

		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		// Quality 0.5 — only Common eligible (MinQuality=0.0)
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 0.5f);
		Ctx.BandEligibilityQuality = 0.5f;

		for (int32 i = 0; i < 50; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num()));
			ASSERT_THAT(AreEqual(0, Evals[0].SelectedBandIndex, TEXT("Only Common should be eligible")));

			FArcItemSpec OutSpec;
			TArray<const FArcItemData*> Ingredients;
			TArray<float> QualityMults;
			FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 0.5f);

			const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(StatsFragment));
			ASSERT_THAT(IsNear(5.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
				TEXT("Should always get Common band stat=5.0")));
		}
	}
};

// ===================================================================
// Integration: Full pipeline (table + context + evaluate + apply)
// ===================================================================

TEST_CLASS(MaterialCraft_FullPipeline, "ArcCraft.MaterialCraft.Integration.FullPipeline")
{
	TEST_METHOD(FullPipeline_TwoRules_ThreeBandsEach_ProducesOutput)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.1f;
		Table->ExtraTimeWeightBonusCap = 1.0f;

		// Rule 1: Metal => strength stats in 3 tiers
		{
			FGameplayTagContainer MetalTags;
			MetalTags.AddTag(TAG_IntTest_Resource_Metal);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Metal Strength");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
			Rule.MaxContributions = 1;

			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Common"), 0.0f, 10.0f, 0.0f, 5.0f, 1.0f));
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Uncommon"), 1.5f, 5.0f, 1.0f, 15.0f, 1.0f));
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Rare"), 3.0f, 1.0f, 2.0f, 50.0f, 1.0f));

			Table->Rules.Add(Rule);
		}

		// Rule 2: Gem => brilliance stat in 2 tiers
		{
			FGameplayTagContainer GemTags;
			GemTags.AddTag(TAG_IntTest_Resource_Gem);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Gem Brilliance");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(GemTags);
			Rule.MaxContributions = 1;

			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Dull"), 0.0f, 5.0f, 0.0f, 3.0f, 0.5f));
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Brilliant"), 2.0f, 2.0f, 1.5f, 20.0f, 1.0f));

			Table->Rules.Add(Rule);
		}

		// Simulate crafting with Metal+Gem ingredients
		FArcItemData ItemA;
		ItemA.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		FArcItemData ItemB;
		ItemB.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Gem_Ruby);

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB };
		TArray<float> QualityMults = { 1.3f, 1.5f };
		const float AvgQuality = 1.4f;

		// Build context via the real Build() path
		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, AvgQuality, FGameplayTagContainer(), /*BaseIngredientCount=*/ 2);

		// Compute quality and weight bonus
		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("FullPipeline: AvgQ=%.2f, BandEligQ=%.2f, BandWeightBonus=%.2f, Ingredients=%d"),
			Ctx.AverageQuality, Ctx.BandEligibilityQuality, Ctx.BandWeightBonus, Ctx.IngredientCount);

		// Evaluate rules
		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		// Both rules should match (Metal.Iron matches Resource.Metal, Gem.Ruby matches Resource.Gem)
		ASSERT_THAT(AreEqual(2, Evals.Num(), TEXT("Both Metal and Gem rules should match")));

		// Apply to output
		FArcItemSpec OutSpec;
		FArcMaterialCraftEvaluator::ApplyEvaluations(
			Evals, OutSpec, Ingredients, QualityMults, Ctx.BandEligibilityQuality);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("FullPipeline"));

		// Verify we have stats — 2 rules, 1 contribution each, 1 stat per band = 2 total
		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment, TEXT("Full pipeline should produce stats")));
		ASSERT_THAT(AreEqual(2, StatsFragment->DefaultStats.Num(),
			TEXT("Two rules with 1 contribution each should produce 2 stats")));
	}

	TEST_METHOD(FullPipeline_ExtraIngredients_DoNotUnlockBands_ButIncreaseWeightBonus)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.5f;
		Table->ExtraTimeWeightBonusCap = 1.0f;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 10.0f, 1.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Rare"), 3.0f, 100.0f, 0.0f, 100.0f, 1.0f));

		Table->Rules.Add(Rule);

		// 4 ingredients, base=2 => 2 extra => +1.0 weight bonus
		FArcItemData ItemA, ItemB, ItemC, ItemD;
		ItemA.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		ItemB.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal);
		ItemC.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal);
		ItemD.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal);

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB, &ItemC, &ItemD };
		TArray<float> QualityMults = { 1.0f, 1.0f, 1.0f, 1.0f };
		const float AvgQuality = 2.5f;

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, AvgQuality, FGameplayTagContainer(), /*BaseIngredientCount=*/ 2);
		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Ctx);

		// BandEligibilityQuality = AverageQuality (NOT boosted by extras)
		ASSERT_THAT(IsNear(2.5f, Ctx.BandEligibilityQuality, 0.001f,
			TEXT("BandEligibilityQuality should equal AverageQuality (2.5), not boosted by extras")));

		// BandWeightBonus = extras from ingredients: max(0, 4-2)*0.5 = 1.0
		ASSERT_THAT(IsNear(1.0f, Ctx.BandWeightBonus, 0.001f,
			TEXT("BandWeightBonus should be 1.0 from extra ingredients")));

		// At BandEligibilityQuality=2.5, Rare band (MinQ=3.0) should NOT be eligible
		// Only Common (MinQ=0.0) is eligible
		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num()));

		// Run multiple times, Rare should NEVER be selected (MinQ=3.0 > BandEligQ=2.5)
		int32 RareCount = 0;
		for (int32 i = 0; i < 50; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> RunEvals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			if (RunEvals.Num() > 0 && RunEvals[0].SelectedBandIndex == 1)
			{
				RareCount++;
			}
		}
		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  ExtraIngredients: Rare selected %d/50 times (should be 0, extras affect weight only)"), RareCount);
		ASSERT_THAT(AreEqual(0, RareCount,
			TEXT("Rare should never be selected — extras affect weight, not band eligibility")));

		// Verify output — should always be Common band
		FArcItemSpec OutSpec;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, Ctx.BandEligibilityQuality);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("ExtraIngredients_WeightBonusOnly"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num()));
	}

	TEST_METHOD(FullPipeline_ComboRule_OnlyFiresWithBothIngredients)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		// Combo rule: Metal AND Gem => synergy stat
		FGameplayTagContainer ComboTags;
		ComboTags.AddTag(TAG_IntTest_Resource_Metal);
		ComboTags.AddTag(TAG_IntTest_Resource_Gem);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal+Gem Synergy");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(ComboTags);
		Rule.MaxContributions = 1;
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Synergy"), 0.0f, 1.0f, 0.0f, 99.0f, 0.0f));
		Table->Rules.Add(Rule);

		// Test with only Metal — should NOT fire
		{
			FArcItemData ItemA;
			ItemA.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);

			TArray<const FArcItemData*> Ingredients = { &ItemA };
			TArray<float> QualityMults = { 1.0f };

			FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
				Ingredients, QualityMults, 1.0f, FGameplayTagContainer());
			Ctx.BandEligibilityQuality = 1.0f;

			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(0, Evals.Num(), TEXT("Metal only should not trigger combo rule")));
		}

		// Test with Metal + Gem — SHOULD fire
		{
			FArcItemData ItemA;
			ItemA.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
			FArcItemData ItemB;
			ItemB.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Gem_Ruby);

			TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB };
			TArray<float> QualityMults = { 1.0f, 1.0f };

			FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
				Ingredients, QualityMults, 1.0f, FGameplayTagContainer());
			Ctx.BandEligibilityQuality = 1.0f;

			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Metal+Gem should trigger combo rule")));

			FArcItemSpec OutSpec;
			FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

			IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("ComboRule_BothPresent"));

			const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(StatsFragment));
			ASSERT_THAT(IsNear(99.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
				TEXT("Combo synergy stat should be 99.0")));
		}
	}

	TEST_METHOD(FullPipeline_MixedRules_IndividualPlusCombo)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		// Individual Metal rule
		{
			FGameplayTagContainer MetalTags;
			MetalTags.AddTag(TAG_IntTest_Resource_Metal);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Metal Strength");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
			Rule.MaxContributions = 1;
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 10.0f, 0.0f));
			Table->Rules.Add(Rule);
		}

		// Individual Gem rule
		{
			FGameplayTagContainer GemTags;
			GemTags.AddTag(TAG_IntTest_Resource_Gem);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Gem Brilliance");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(GemTags);
			Rule.MaxContributions = 1;
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 8.0f, 0.0f));
			Table->Rules.Add(Rule);
		}

		// Combo rule: Metal+Gem synergy
		{
			FGameplayTagContainer ComboTags;
			ComboTags.AddTag(TAG_IntTest_Resource_Metal);
			ComboTags.AddTag(TAG_IntTest_Resource_Gem);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Metal+Gem Synergy");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAllTags(ComboTags);
			Rule.MaxContributions = 1;
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Synergy"), 0.0f, 1.0f, 0.0f, 30.0f, 0.0f));
			Table->Rules.Add(Rule);
		}

		// Craft with Metal + Gem => all 3 rules should fire
		FArcItemData ItemA;
		ItemA.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		FArcItemData ItemB;
		ItemB.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Gem_Ruby);

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB };
		TArray<float> QualityMults = { 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer());
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(3, Evals.Num(), TEXT("Metal + Gem + Combo = 3 rules")));

		FArcItemSpec OutSpec;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("MixedRules_IndividualPlusCombo"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(3, StatsFragment->DefaultStats.Num(),
			TEXT("3 rules, each contributing 1 stat = 3 stats total")));

		// Verify stat values (all have zero quality scaling, so raw base values)
		// Order depends on rule evaluation order, which is priority-based (all priority 0, so table order)
		ASSERT_THAT(IsNear(8.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f));
		ASSERT_THAT(IsNear(30.0f, StatsFragment->DefaultStats[1].Value.GetValue(), 0.001f));
		ASSERT_THAT(IsNear(10.0f, StatsFragment->DefaultStats[2].Value.GetValue(), 0.001f));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  MixedRules totals: Metal=%.1f, Gem=%.1f, Synergy=%.1f (sum=%.1f)"),
			StatsFragment->DefaultStats[0].Value.GetValue(),
			StatsFragment->DefaultStats[1].Value.GetValue(),
			StatsFragment->DefaultStats[2].Value.GetValue(),
			StatsFragment->DefaultStats[0].Value.GetValue()
				+ StatsFragment->DefaultStats[1].Value.GetValue()
				+ StatsFragment->DefaultStats[2].Value.GetValue());
	}

	TEST_METHOD(FullPipeline_MaxActiveRules_LimitsTotalModifiers)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();
		Table->MaxActiveRules = 1; // Only 1 rule can fire

		// Rule 1: Metal (priority 5)
		{
			FGameplayTagContainer MetalTags;
			MetalTags.AddTag(TAG_IntTest_Resource_Metal);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Metal High Priority");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
			Rule.Priority = 5;
			Rule.MaxContributions = 1;
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 10.0f, 0.0f));
			Table->Rules.Add(Rule);
		}

		// Rule 2: Gem (priority 1 — lower, should be excluded)
		{
			FGameplayTagContainer GemTags;
			GemTags.AddTag(TAG_IntTest_Resource_Gem);

			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString("Gem Low Priority");
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(GemTags);
			Rule.Priority = 1;
			Rule.MaxContributions = 1;
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 20.0f, 0.0f));
			Table->Rules.Add(Rule);
		}

		FArcItemData ItemA;
		ItemA.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal);
		FArcItemData ItemB;
		ItemB.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Gem);

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB };
		TArray<float> QualityMults = { 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer());
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("MaxActiveRules=1 should limit to 1 evaluation")));

		FArcItemSpec OutSpec;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("MaxActiveRules_Limit"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num(),
			TEXT("Only 1 rule should contribute stats")));
		// Higher priority (Metal, priority=5) should win
		ASSERT_THAT(IsNear(10.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Higher priority Metal rule (stat=10) should win over Gem (stat=20)")));
	}
};

// ===================================================================
// Integration: Band with MultiStat modifier (a single modifier with multiple stats)
// ===================================================================

TEST_CLASS(MaterialCraft_MultiStatModifier, "ArcCraft.MaterialCraft.Integration.MultiStat")
{
	TEST_METHOD(BandWithMultiStatModifier_AllStatsAdded)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Multi-Stat");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;

		// Band with one modifier that has 3 base stats
		TArray<FInstancedStruct> Modifiers;
		Modifiers.Add(IntegrationTestHelpers::MakeMultiStatModifier({ 10.0f, 20.0f, 5.0f }, 0.0f));

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithStats(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, Modifiers));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num()));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("MultiStatModifier"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(3, StatsFragment->DefaultStats.Num(),
			TEXT("Multi-stat modifier should produce 3 stats")));
		ASSERT_THAT(IsNear(10.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f));
		ASSERT_THAT(IsNear(20.0f, StatsFragment->DefaultStats[1].Value.GetValue(), 0.001f));
		ASSERT_THAT(IsNear(5.0f, StatsFragment->DefaultStats[2].Value.GetValue(), 0.001f));
	}

	TEST_METHOD(TwoContributions_MultiStatModifier_AccumulatesStats)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Multi-Stat x2");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 2;

		TArray<FInstancedStruct> Modifiers;
		Modifiers.Add(IntegrationTestHelpers::MakeMultiStatModifier({ 10.0f, 20.0f }, 0.0f));

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithStats(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, Modifiers));
		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(2, Evals.Num(), TEXT("MaxContributions=2")));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("MultiStat_x2_Accumulate"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		// 2 contributions * 2 stats each = 4 total stats
		ASSERT_THAT(AreEqual(4, StatsFragment->DefaultStats.Num(),
			TEXT("2 contributions of 2-stat modifier should give 4 stats total")));
	}
};

// ===================================================================
// Material Craft: Selective band/rule tests — 5 rules with modifiers,
// but only a subset fires due to MaxActiveRules or Budget constraints.
// This tests the real use case: many possible outcomes, few selected.
// ===================================================================

TEST_CLASS(MaterialCraft_SelectiveRules, "ArcCraft.MaterialCraft.Integration.SelectiveRules")
{
	/**
	 * Helper: Creates a table with 5 rules, each matching "Resource.Metal"
	 * at different priorities. Each rule has a single band with a unique stat value
	 * so we can identify which rules fired by inspecting output stat values.
	 *
	 * Rule 0: "Hardness"    priority=5, stat=100.0
	 * Rule 1: "Sharpness"   priority=4, stat=200.0
	 * Rule 2: "Resilience"  priority=3, stat=300.0
	 * Rule 3: "Conductance" priority=2, stat=400.0
	 * Rule 4: "Luster"      priority=1, stat=500.0
	 */
	UArcMaterialPropertyTable* CreateFiveRuleTable()
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		const TCHAR* Names[] = { TEXT("Hardness"), TEXT("Sharpness"), TEXT("Resilience"), TEXT("Conductance"), TEXT("Luster") };
		const float StatValues[] = { 100.0f, 200.0f, 300.0f, 400.0f, 500.0f };
		const int32 Priorities[] = { 5, 4, 3, 2, 1 };

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		for (int32 i = 0; i < 5; ++i)
		{
			FArcMaterialPropertyRule Rule;
			Rule.RuleName = FText::FromString(Names[i]);
			Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
			Rule.Priority = Priorities[i];
			Rule.MaxContributions = 1;
			Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
				FText::FromString(TEXT("Common")), 0.0f, 1.0f, 0.0f, StatValues[i], 0.0f));
			Table->Rules.Add(Rule);
		}

		return Table;
	}

	TEST_METHOD(FiveRulesMatch_MaxActive1_OnlyHighestPriorityFires)
	{
		UArcMaterialPropertyTable* Table = CreateFiveRuleTable();
		Table->MaxActiveRules = 1;

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("MaxActiveRules=1 should limit to 1")));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("5Rules_MaxActive1"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num(),
			TEXT("Only 1 rule should fire")));
		// Priority 5 = "Hardness" with stat=100.0
		ASSERT_THAT(IsNear(100.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Highest priority rule (Hardness=100) should be selected")));
	}

	TEST_METHOD(FiveRulesMatch_MaxActive2_TopTwoPrioritiesFire)
	{
		UArcMaterialPropertyTable* Table = CreateFiveRuleTable();
		Table->MaxActiveRules = 2;

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(2, Evals.Num(), TEXT("MaxActiveRules=2")));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("5Rules_MaxActive2"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(2, StatsFragment->DefaultStats.Num(),
			TEXT("2 rules should fire")));

		// Collect stat values
		TSet<int32> ObservedStatValues;
		for (int32 i = 0; i < StatsFragment->DefaultStats.Num(); ++i)
		{
			ObservedStatValues.Add(FMath::RoundToInt32(StatsFragment->DefaultStats[i].Value.GetValue()));
		}
		// Priority 5=Hardness(100), Priority 4=Sharpness(200) should be selected
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(100), TEXT("Hardness(100) should fire")));
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(200), TEXT("Sharpness(200) should fire")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  5Rules_MaxActive2: selected stats = %d, %d"),
			FMath::RoundToInt32(StatsFragment->DefaultStats[0].Value.GetValue()),
			FMath::RoundToInt32(StatsFragment->DefaultStats[1].Value.GetValue()));
	}

	TEST_METHOD(FiveRulesMatch_MaxActive3_TopThreePrioritiesFire)
	{
		UArcMaterialPropertyTable* Table = CreateFiveRuleTable();
		Table->MaxActiveRules = 3;

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(3, Evals.Num(), TEXT("MaxActiveRules=3")));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("5Rules_MaxActive3"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(3, StatsFragment->DefaultStats.Num(),
			TEXT("3 rules should fire")));

		TSet<int32> ObservedStatValues;
		for (int32 i = 0; i < StatsFragment->DefaultStats.Num(); ++i)
		{
			ObservedStatValues.Add(FMath::RoundToInt32(StatsFragment->DefaultStats[i].Value.GetValue()));
		}
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(100), TEXT("Hardness(100) should fire")));
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(200), TEXT("Sharpness(200) should fire")));
		ASSERT_THAT(IsTrue(ObservedStatValues.Contains(300), TEXT("Resilience(300) should fire")));
		// 400 and 500 should NOT be present
		ASSERT_THAT(IsFalse(ObservedStatValues.Contains(400), TEXT("Conductance(400) should NOT fire")));
		ASSERT_THAT(IsFalse(ObservedStatValues.Contains(500), TEXT("Luster(500) should NOT fire")));
	}

	TEST_METHOD(FiveRulesMatch_Unlimited_AllFive)
	{
		UArcMaterialPropertyTable* Table = CreateFiveRuleTable();
		Table->MaxActiveRules = 0; // unlimited

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
		ASSERT_THAT(AreEqual(5, Evals.Num(), TEXT("Unlimited should allow all 5 rules")));

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;
		FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

		IntegrationTestHelpers::LogEvaluationResults(Evals, OutSpec, TEXT("5Rules_Unlimited"));

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(5, StatsFragment->DefaultStats.Num(),
			TEXT("All 5 rules should produce 5 stats")));

		float Sum = 0.0f;
		for (int32 i = 0; i < StatsFragment->DefaultStats.Num(); ++i)
		{
			Sum += StatsFragment->DefaultStats[i].Value.GetValue();
		}
		// All stats: 100+200+300+400+500 = 1500
		ASSERT_THAT(IsNear(1500.0f, Sum, 0.01f, TEXT("Sum of all 5 stats should be 1500")));
	}
};

// ===================================================================
// Material Craft: Single rule with 5 bands, quality gates only allow
// a subset to be eligible. Tests that band selection among many bands
// correctly respects MinQuality thresholds.
// ===================================================================

TEST_CLASS(MaterialCraft_SelectiveBands, "ArcCraft.MaterialCraft.Integration.SelectiveBands")
{
	/**
	 * Helper: Creates a single rule with 5 bands at increasing quality thresholds.
	 * Each band has a unique stat value for identification.
	 *
	 * Band 0: "Trash"     MinQ=0.0,  stat=10.0
	 * Band 1: "Common"    MinQ=1.0,  stat=20.0
	 * Band 2: "Uncommon"  MinQ=2.0,  stat=50.0
	 * Band 3: "Rare"      MinQ=3.5,  stat=100.0
	 * Band 4: "Legendary" MinQ=5.0,  stat=250.0
	 */
	UArcMaterialPropertyTable* CreateFiveBandTable()
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Five-Tier");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;

		// All bands have equal weights and no bias, so selection is pure uniform among eligible
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Trash"), 0.0f, 1.0f, 0.0f, 10.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 1.0f, 1.0f, 0.0f, 20.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Uncommon"), 2.0f, 1.0f, 0.0f, 50.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Rare"), 3.5f, 1.0f, 0.0f, 100.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Legendary"), 5.0f, 1.0f, 0.0f, 250.0f, 0.0f));

		Table->Rules.Add(Rule);
		return Table;
	}

	TEST_METHOD(VeryLowQuality_OnlyTrashEligible)
	{
		UArcMaterialPropertyTable* Table = CreateFiveBandTable();

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		// Quality 0.5 => only Band 0 (MinQ=0.0) eligible
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 0.5f);
		Ctx.BandEligibilityQuality = 0.5f;

		TSet<int32> ObservedBands;
		for (int32 i = 0; i < 50; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num()));
			ObservedBands.Add(Evals[0].SelectedBandIndex);

			FArcItemSpec OutSpec;
			TArray<const FArcItemData*> Ingredients;
			TArray<float> QualityMults;
			FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 0.5f);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(1, Stats->DefaultStats.Num()));
			ASSERT_THAT(IsNear(10.0f, Stats->DefaultStats[0].Value.GetValue(), 0.001f,
				TEXT("Only Trash band (stat=10) should be selected")));
		}

		// Should only ever see band 0
		ASSERT_THAT(AreEqual(1, ObservedBands.Num(), TEXT("Only 1 band should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Band 0 (Trash) must be the only one")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  VeryLowQuality: only band 0 observed across 50 runs (expected)"));
	}

	TEST_METHOD(MidQuality_ThreeBandsEligible)
	{
		UArcMaterialPropertyTable* Table = CreateFiveBandTable();

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		// Quality 2.5 => Bands 0 (MinQ=0), 1 (MinQ=1), 2 (MinQ=2) are eligible
		// Bands 3 (MinQ=3.5) and 4 (MinQ=5.0) are NOT eligible
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 2.5f);
		Ctx.BandEligibilityQuality = 2.5f;

		TSet<int32> ObservedBands;
		TMap<int32, int32> BandCounts;

		for (int32 i = 0; i < 200; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num()));

			const int32 Band = Evals[0].SelectedBandIndex;
			ObservedBands.Add(Band);
			BandCounts.FindOrAdd(Band, 0)++;

			FArcItemSpec OutSpec;
			TArray<const FArcItemData*> Ingredients;
			TArray<float> QualityMults;
			FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 2.5f);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(1, Stats->DefaultStats.Num(), TEXT("One band = one stat")));

			// Stat should be one of: 10, 20, 50
			const float V = Stats->DefaultStats[0].Value.GetValue();
			const bool bValid = FMath::IsNearlyEqual(V, 10.0f, 0.1f)
				|| FMath::IsNearlyEqual(V, 20.0f, 0.1f)
				|| FMath::IsNearlyEqual(V, 50.0f, 0.1f);
			ASSERT_THAT(IsTrue(bValid,
				TEXT("Stat must be 10 (Trash), 20 (Common), or 50 (Uncommon)")));
		}

		// With equal weights and 200 runs, all 3 eligible bands should appear
		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Trash should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(1), TEXT("Common should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(2), TEXT("Uncommon should appear")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(3), TEXT("Rare should NOT appear")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(4), TEXT("Legendary should NOT appear")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  MidQuality: Trash=%d, Common=%d, Uncommon=%d, Rare=%d, Legendary=%d over 200 runs"),
			BandCounts.FindRef(0), BandCounts.FindRef(1), BandCounts.FindRef(2),
			BandCounts.FindRef(3), BandCounts.FindRef(4));
	}

	TEST_METHOD(HighQuality_AllFiveBandsEligible)
	{
		UArcMaterialPropertyTable* Table = CreateFiveBandTable();

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		// Quality 10.0 => all 5 bands eligible
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 10.0f);
		Ctx.BandEligibilityQuality = 10.0f;

		TSet<int32> ObservedBands;
		TMap<int32, int32> BandCounts;

		for (int32 i = 0; i < 200; ++i)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(1, Evals.Num()));

			const int32 Band = Evals[0].SelectedBandIndex;
			ObservedBands.Add(Band);
			BandCounts.FindOrAdd(Band, 0)++;

			FArcItemSpec OutSpec;
			TArray<const FArcItemData*> Ingredients;
			TArray<float> QualityMults;
			FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 10.0f);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(1, Stats->DefaultStats.Num()));
		}

		// All 5 should have been selected at least once
		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Trash should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(1), TEXT("Common should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(2), TEXT("Uncommon should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(3), TEXT("Rare should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(4), TEXT("Legendary should appear")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  HighQuality: Trash=%d, Common=%d, Uncommon=%d, Rare=%d, Legendary=%d over 200 runs"),
			BandCounts.FindRef(0), BandCounts.FindRef(1), BandCounts.FindRef(2),
			BandCounts.FindRef(3), BandCounts.FindRef(4));
	}

	TEST_METHOD(MultiContribution_FiveBands_EachContributionSelectsOneBand)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Five-Tier Multi");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 3; // 3 rolls into 5 bands

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Trash"), 0.0f, 1.0f, 0.0f, 10.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 1.0f, 1.0f, 0.0f, 20.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Uncommon"), 2.0f, 1.0f, 0.0f, 50.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Rare"), 3.5f, 1.0f, 0.0f, 100.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Legendary"), 5.0f, 1.0f, 0.0f, 250.0f, 0.0f));

		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		// BandEligibilityQuality 10.0 => all bands eligible, 3 contributions
		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 10.0f);
		Ctx.BandEligibilityQuality = 10.0f;

		// Run multiple times to check distribution
		int32 TotalStats = 0;
		TMap<int32, int32> BandCounts;

		for (int32 Run = 0; Run < 100; ++Run)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
			ASSERT_THAT(AreEqual(3, Evals.Num(), TEXT("3 contributions expected")));

			for (const auto& Eval : Evals)
			{
				BandCounts.FindOrAdd(Eval.SelectedBandIndex, 0)++;
			}

			FArcItemSpec OutSpec;
			TArray<const FArcItemData*> Ingredients;
			TArray<float> QualityMults;
			FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 10.0f);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			// Each contribution selects one band with one stat modifier
			ASSERT_THAT(AreEqual(3, Stats->DefaultStats.Num(),
				TEXT("3 contributions should produce 3 stats")));
			TotalStats += Stats->DefaultStats.Num();
		}

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  MultiContrib 5-Band: TotalStats=%d over 100 runs (expected=300)"), TotalStats);
		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("    BandDistribution: Trash=%d, Common=%d, Uncommon=%d, Rare=%d, Legendary=%d"),
			BandCounts.FindRef(0), BandCounts.FindRef(1), BandCounts.FindRef(2),
			BandCounts.FindRef(3), BandCounts.FindRef(4));
	}

	TEST_METHOD(Budget_FiveBands_LimitsSelection)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();
		// Budget mode: BaseBandBudget=3.0, BudgetPerQuality=0.0
		// Band costs: band0=1, band1=2, band2=3, band3=4, band4=5
		// Budget=3.0 means: can afford band0(1) + band1(2) = 3, or band2(3) alone, etc.
		Table->BaseBandBudget = 3.0f;
		Table->BudgetPerQuality = 0.0f;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Budgeted");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 5; // Wants to contribute 5 times, but budget limits it

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Trash"), 0.0f, 1.0f, 0.0f, 10.0f, 0.0f));     // cost=1
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 20.0f, 0.0f));    // cost=2
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Uncommon"), 0.0f, 1.0f, 0.0f, 50.0f, 0.0f));  // cost=3
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Rare"), 0.0f, 1.0f, 0.0f, 100.0f, 0.0f));     // cost=4
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Legendary"), 0.0f, 1.0f, 0.0f, 250.0f, 0.0f));// cost=5

		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
		Ctx.BandEligibilityQuality = 1.0f;

		// Run multiple times; budget=3 means total band costs must stay <=3
		int32 MaxEvals = 0;
		int32 MinEvals = INT_MAX;

		for (int32 Run = 0; Run < 100; ++Run)
		{
			TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);

			// Budget=3, so we can get at most 3 evaluations (3x band0 at cost=1 each)
			// and can never get 5 evaluations
			ASSERT_THAT(IsTrue(Evals.Num() <= 3,
				TEXT("Budget=3 should not allow more than 3 evaluations")));

			MaxEvals = FMath::Max(MaxEvals, Evals.Num());
			MinEvals = FMath::Min(MinEvals, Evals.Num());

			// Verify the total budget spent
			float TotalCost = 0.0f;
			for (const auto& Eval : Evals)
			{
				TotalCost += static_cast<float>(Eval.SelectedBandIndex + 1);
			}
			ASSERT_THAT(IsTrue(TotalCost <= 3.0f + 0.001f,
				TEXT("Total band cost should not exceed budget")));

			FArcItemSpec OutSpec;
			TArray<const FArcItemData*> Ingredients;
			TArray<float> QualityMults;
			FArcMaterialCraftEvaluator::ApplyEvaluations(Evals, OutSpec, Ingredients, QualityMults, 1.0f);

			const FArcItemFragment_ItemStats* Stats = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			if (Evals.Num() > 0)
			{
				ASSERT_THAT(IsNotNull(Stats));
				ASSERT_THAT(AreEqual(Evals.Num(), Stats->DefaultStats.Num(),
					TEXT("Each evaluation should produce one stat")));
			}
		}

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  Budget_5Bands: min evals=%d, max evals=%d over 100 runs (budget=3.0)"),
			MinEvals, MaxEvals);
	}

	TEST_METHOD(Budget_IncreasesWithQuality_AllowsMoreBands)
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();
		// Budget starts at 1.0, gains 2.0 per quality above 1.0
		// At quality 1.0: budget = 1.0 + 0*2 = 1.0 (only band0 affordable)
		// At quality 3.0: budget = 1.0 + 2.0*2 = 5.0 (can afford band4 or multiple cheaper)
		Table->BaseBandBudget = 1.0f;
		Table->BudgetPerQuality = 2.0f;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Budget Scaling");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 5;

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Trash"), 0.0f, 1.0f, 0.0f, 10.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Common"), 0.0f, 1.0f, 0.0f, 20.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Uncommon"), 0.0f, 1.0f, 0.0f, 50.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Rare"), 0.0f, 1.0f, 0.0f, 100.0f, 0.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Legendary"), 0.0f, 1.0f, 0.0f, 250.0f, 0.0f));

		Table->Rules.Add(Rule);

		FGameplayTagContainer IngredientTags;
		IngredientTags.AddTag(TAG_IntTest_Resource_Metal);

		// Low quality: budget=1, can only afford band 0 (cost=1), once
		{
			FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 1.0f);
			Ctx.BandEligibilityQuality = 1.0f;

			int32 MaxEvals = 0;
			for (int32 Run = 0; Run < 50; ++Run)
			{
				TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
				MaxEvals = FMath::Max(MaxEvals, Evals.Num());
				ASSERT_THAT(AreEqual(1, Evals.Num(), TEXT("Budget=1.0 should allow exactly 1 eval at band0")));
			}
			UE_LOG(LogArcCraftIntegrationTest, Log,
				TEXT("  BudgetScaling: lowQ max evals=%d (expected 1)"), MaxEvals);
		}

		// High quality: budget=5, can afford multiple
		{
			FArcMaterialCraftContext Ctx = IntegrationTestHelpers::MakeContext(IngredientTags, 3.0f);
			Ctx.BandEligibilityQuality = 3.0f;

			int32 MaxEvals = 0;
			int32 MinEvals = INT_MAX;
			for (int32 Run = 0; Run < 100; ++Run)
			{
				TArray<FArcMaterialRuleEvaluation> Evals = FArcMaterialCraftEvaluator::EvaluateRules(Table, Ctx);
				MaxEvals = FMath::Max(MaxEvals, Evals.Num());
				MinEvals = FMath::Min(MinEvals, Evals.Num());
				// Budget=5, so at most 5 evals (5x band0) but likely less
				ASSERT_THAT(IsTrue(Evals.Num() >= 1, TEXT("High budget should allow at least 1")));
			}
			// With budget=5 and 5 bands of varying cost, should see more than 1 eval sometimes
			ASSERT_THAT(IsTrue(MaxEvals > 1,
				TEXT("Budget=5 should sometimes allow more than 1 evaluation")));
			UE_LOG(LogArcCraftIntegrationTest, Log,
				TEXT("  BudgetScaling: highQ min=%d, max=%d (budget=5.0)"), MinEvals, MaxEvals);
		}
	}
};

// ===================================================================
// Realistic Quality Pipeline: Items → TierTable → QualityMults →
// AverageQuality → BandEligibilityQuality + BandWeightBonus → Band Selection
//
// These tests demonstrate the real crafting flow where the player
// adds items with tier tags to the crafting menu, and the system
// computes quality from those items rather than hardcoding values.
// ===================================================================

// Tier tags for quality evaluation
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Material_Tier_Crude, "Material.Tier.Crude");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Material_Tier_Common, "Material.Tier.Common");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Material_Tier_Fine, "Material.Tier.Fine");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Material_Tier_Masterwork, "Material.Tier.Masterwork");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_IntTest_Material_Tier_Legendary, "Material.Tier.Legendary");

namespace RealisticQualityHelpers
{
	/**
	 * Create a quality tier table with 5 tiers representing material grades.
	 * This simulates what a game designer would configure in a data asset:
	 *
	 *   Crude      -> QualityMult 0.6 (below baseline — inferior materials)
	 *   Common     -> QualityMult 1.0 (baseline quality)
	 *   Fine       -> QualityMult 1.4 (crafted with care)
	 *   Masterwork -> QualityMult 2.0 (exceptional materials)
	 *   Legendary  -> QualityMult 3.0 (mythical-grade materials)
	 */
	UArcQualityTierTable* CreateTierTable()
	{
		UArcQualityTierTable* TierTable = NewObject<UArcQualityTierTable>(GetTransientPackage());

		TierTable->Tiers.Add({ TAG_IntTest_Material_Tier_Crude, 0, 0.6f });
		TierTable->Tiers.Add({ TAG_IntTest_Material_Tier_Common, 1, 1.0f });
		TierTable->Tiers.Add({ TAG_IntTest_Material_Tier_Fine, 2, 1.4f });
		TierTable->Tiers.Add({ TAG_IntTest_Material_Tier_Masterwork, 3, 2.0f });
		TierTable->Tiers.Add({ TAG_IntTest_Material_Tier_Legendary, 4, 3.0f });

		return TierTable;
	}

	/**
	 * Create a material property table with a single rule ("Metal Tempering")
	 * that has 4 quality bands:
	 *
	 *   Brittle    MinQ=0.0  stat=5.0   — always eligible, poor result
	 *   Standard   MinQ=1.0  stat=15.0  — baseline quality materials needed
	 *   Tempered   MinQ=1.5  stat=40.0  — requires good materials
	 *   Masterwork MinQ=2.5  stat=100.0 — exceptional materials only
	 *
	 * All bands use QualityScalingFactor=1.0 so stat values scale with quality.
	 * QualityWeightBias=0.5 on higher bands so better quality shifts odds upward.
	 */
	UArcMaterialPropertyTable* CreateWeaponTable()
	{
		UArcMaterialPropertyTable* Table = IntegrationTestHelpers::CreateTransientTable();
		Table->ExtraIngredientWeightBonus = 0.15f;
		Table->ExtraTimeWeightBonusCap = 0.5f;

		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_IntTest_Resource_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString("Metal Tempering");
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.MaxContributions = 1;

		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Brittle"), 0.0f, 10.0f, 0.0f, 5.0f, 1.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Standard"), 1.0f, 5.0f, 0.5f, 15.0f, 1.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Tempered"), 1.5f, 2.0f, 0.5f, 40.0f, 1.0f));
		Rule.QualityBands.Add(IntegrationTestHelpers::MakeBandWithSingleStat(
			FText::FromString("Masterwork"), 2.5f, 1.0f, 1.0f, 100.0f, 1.0f));

		Table->Rules.Add(Rule);
		return Table;
	}

	/**
	 * Simulate the real crafting flow:
	 *  1. Items have resource tags and tier tags
	 *  2. UArcQualityTierTable evaluates each item's tags → quality multiplier
	 *  3. Weighted average of quality mults → AverageQuality
	 *  4. Context::Build() aggregates ingredient tags
	 *  5. ComputeQualityAndWeightBonus() sets BandEligibilityQuality and BandWeightBonus
	 *  6. EvaluateRules() matches rules and selects bands
	 *  7. ApplyEvaluations() writes modifiers to the output item
	 *
	 * Returns the BandEligibilityQuality and BandWeightBonus so tests can verify the full pipeline.
	 */
	struct FRealisticCraftResult
	{
		FArcItemSpec OutSpec;
		FArcMaterialCraftContext Context;
		TArray<FArcMaterialRuleEvaluation> Evaluations;
		float BandEligibilityQuality = 1.0f;
		float BandWeightBonus = 0.0f;
	};

	FRealisticCraftResult RunFullCraftPipeline(
		UArcMaterialPropertyTable* Table,
		UArcQualityTierTable* TierTable,
		const TArray<FArcItemData*>& Items,
		int32 BaseIngredientCount = 0,
		float ExtraCraftTimeBonus = 0.0f)
	{
		FRealisticCraftResult Result;

		// Step 1: Evaluate quality multiplier per ingredient using the tier table
		// This is what ArcCraftExecution_Recipe::CheckAndConsumeRecipeItems does
		TArray<float> QualityMults;
		TArray<const FArcItemData*> ConstIngredients;
		QualityMults.Reserve(Items.Num());
		ConstIngredients.Reserve(Items.Num());

		for (const FArcItemData* Item : Items)
		{
			ConstIngredients.Add(Item);

			// Real system calls: TierTable->EvaluateQuality(Item->GetItemAggregatedTags())
			const float QualityMult = TierTable->EvaluateQuality(Item->GetItemAggregatedTags());
			QualityMults.Add(QualityMult);
		}

		// Step 2: Compute weighted average quality
		// Real system weights by ingredient Amount; here each item = weight 1
		float TotalQuality = 0.0f;
		for (const float Q : QualityMults)
		{
			TotalQuality += Q;
		}
		const float AverageQuality = QualityMults.Num() > 0 ? TotalQuality / QualityMults.Num() : 1.0f;

		// Step 3: Build context (aggregates tags from items)
		Result.Context = FArcMaterialCraftContext::Build(
			ConstIngredients, QualityMults, AverageQuality,
			FGameplayTagContainer(), BaseIngredientCount, ExtraCraftTimeBonus);

		// Step 4: Compute quality and weight bonus
		// Now void — sets BandEligibilityQuality and BandWeightBonus on context
		FArcMaterialCraftEvaluator::ComputeQualityAndWeightBonus(Table, Result.Context);
		Result.BandEligibilityQuality = Result.Context.BandEligibilityQuality;
		Result.BandWeightBonus = Result.Context.BandWeightBonus;

		// Step 5: Evaluate rules → select bands
		Result.Evaluations = FArcMaterialCraftEvaluator::EvaluateRules(Table, Result.Context);

		// Step 6: Apply band modifiers to output item
		FArcMaterialCraftEvaluator::ApplyEvaluations(
			Result.Evaluations, Result.OutSpec,
			ConstIngredients, QualityMults, Result.BandEligibilityQuality);

		return Result;
	}
}

TEST_CLASS(MaterialCraft_RealisticQuality, "ArcCraft.MaterialCraft.Integration.RealisticQuality")
{
	/**
	 * Scenario: Player crafts a weapon using two Crude iron ingots.
	 * Crude tier → quality mult 0.6 each → AverageQuality = 0.6
	 * No extra ingredients or time → BandEligibilityQuality = 0.6, BandWeightBonus = 0
	 * Only "Brittle" band (MinQ=0.0) is eligible. Result is always poor.
	 */
	TEST_METHOD(CrudeIngredients_AlwaysProduceBrittleResult)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		// Two crude iron items
		FArcItemData CrudeIron1;
		CrudeIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CrudeIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Crude);

		FArcItemData CrudeIron2;
		CrudeIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CrudeIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Crude);

		TArray<FArcItemData*> Items = { &CrudeIron1, &CrudeIron2 };

		// Verify quality evaluation
		const float QMult = TierTable->EvaluateQuality(CrudeIron1.GetItemAggregatedTags());
		ASSERT_THAT(IsNear(0.6f, QMult, 0.001f,
			TEXT("Crude tier should evaluate to 0.6 quality multiplier")));

		auto Result = RealisticQualityHelpers::RunFullCraftPipeline(PropTable, TierTable, Items);

		// AvgQ = (0.6 + 0.6) / 2 = 0.6, no extras → BandEligibilityQuality = 0.6
		ASSERT_THAT(IsNear(0.6f, Result.Context.AverageQuality, 0.001f,
			TEXT("Two Crude items → AverageQuality should be 0.6")));
		ASSERT_THAT(IsNear(0.6f, Result.BandEligibilityQuality, 0.001f,
			TEXT("No extras → BandEligibilityQuality should equal AverageQuality")));

		// At BandEligibilityQuality=0.6, only Brittle (MinQ=0.0) is eligible — Standard needs 1.0
		ASSERT_THAT(AreEqual(1, Result.Evaluations.Num()));
		ASSERT_THAT(AreEqual(0, Result.Evaluations[0].SelectedBandIndex,
			TEXT("Crude materials should only reach Brittle band")));

		// Stat: 5.0 * (1 + (0.6-1.0)*1.0) = 5.0 * 0.6 = 3.0
		// Quality scaling: FinalValue = BaseValue * (1 + (AvgQ - 1) * ScalingFactor)
		const FArcItemFragment_ItemStats* Stats = Result.OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(Stats));
		const float ExpectedStat = 5.0f * (1.0f + (0.6f - 1.0f) * 1.0f);
		ASSERT_THAT(IsNear(ExpectedStat, Stats->DefaultStats[0].Value.GetValue(), 0.01f,
			TEXT("Stat should be scaled down by low quality")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  CrudeIngredients: QMult=%.2f, AvgQ=%.2f, BandEligQ=%.2f, Band=%d, Stat=%.2f"),
			QMult, Result.Context.AverageQuality, Result.BandEligibilityQuality,
			Result.Evaluations[0].SelectedBandIndex,
			Stats->DefaultStats[0].Value.GetValue());
	}

	/**
	 * Scenario: Player crafts with two Common iron ingots.
	 * Common tier → quality mult 1.0 each → AverageQuality = 1.0
	 * BandEligibilityQuality = 1.0 → "Brittle" and "Standard" bands eligible.
	 * Standard has lower weight than Brittle, so both appear across runs.
	 */
	TEST_METHOD(CommonIngredients_BrittleAndStandardEligible)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		FArcItemData CommonIron1;
		CommonIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);

		FArcItemData CommonIron2;
		CommonIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);

		TArray<FArcItemData*> Items = { &CommonIron1, &CommonIron2 };

		// Verify tier evaluation
		ASSERT_THAT(IsNear(1.0f, TierTable->EvaluateQuality(CommonIron1.GetItemAggregatedTags()), 0.001f));

		TSet<int32> ObservedBands;
		TMap<int32, int32> BandCounts;

		for (int32 Run = 0; Run < 100; ++Run)
		{
			auto Result = RealisticQualityHelpers::RunFullCraftPipeline(PropTable, TierTable, Items);

			ASSERT_THAT(IsNear(1.0f, Result.Context.AverageQuality, 0.001f));
			ASSERT_THAT(IsNear(1.0f, Result.BandEligibilityQuality, 0.001f));
			ASSERT_THAT(AreEqual(1, Result.Evaluations.Num()));

			const int32 Band = Result.Evaluations[0].SelectedBandIndex;
			ObservedBands.Add(Band);
			BandCounts.FindOrAdd(Band, 0)++;

			// Must be Brittle(0) or Standard(1) — Tempered(2) needs MinQ=1.5
			ASSERT_THAT(IsTrue(Band <= 1,
				TEXT("At quality 1.0 only Brittle and Standard should be eligible")));
		}

		// Both bands should appear across 100 runs
		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Brittle band should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(1), TEXT("Standard band should appear")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(2), TEXT("Tempered should NOT appear")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(3), TEXT("Masterwork should NOT appear")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  CommonIngredients: Brittle=%d, Standard=%d over 100 runs"),
			BandCounts.FindRef(0), BandCounts.FindRef(1));
	}

	/**
	 * Scenario: Player uses one Fine iron and one Masterwork iron.
	 * Fine=1.4, Masterwork=2.0 → AverageQuality = (1.4+2.0)/2 = 1.7
	 * BandEligibilityQuality=1.7 → Brittle(0.0), Standard(1.0), Tempered(1.5) eligible.
	 * Masterwork band (MinQ=2.5) NOT eligible.
	 *
	 * Shows: mixing material tiers affects the quality average.
	 */
	TEST_METHOD(MixedTierIngredients_BlendedQualityAffectsBandAccess)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		FArcItemData FineIron;
		FineIron.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		FineIron.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Fine);

		FArcItemData MasterworkIron;
		MasterworkIron.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		MasterworkIron.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Masterwork);

		TArray<FArcItemData*> Items = { &FineIron, &MasterworkIron };

		// Verify per-item quality
		ASSERT_THAT(IsNear(1.4f, TierTable->EvaluateQuality(FineIron.GetItemAggregatedTags()), 0.001f,
			TEXT("Fine tier should evaluate to 1.4")));
		ASSERT_THAT(IsNear(2.0f, TierTable->EvaluateQuality(MasterworkIron.GetItemAggregatedTags()), 0.001f,
			TEXT("Masterwork tier should evaluate to 2.0")));

		TSet<int32> ObservedBands;
		TMap<int32, int32> BandCounts;

		for (int32 Run = 0; Run < 150; ++Run)
		{
			auto Result = RealisticQualityHelpers::RunFullCraftPipeline(PropTable, TierTable, Items);

			// AverageQuality should be (1.4 + 2.0) / 2 = 1.7
			ASSERT_THAT(IsNear(1.7f, Result.Context.AverageQuality, 0.001f));
			ASSERT_THAT(IsNear(1.7f, Result.BandEligibilityQuality, 0.001f));

			ASSERT_THAT(AreEqual(1, Result.Evaluations.Num()));
			const int32 Band = Result.Evaluations[0].SelectedBandIndex;
			ObservedBands.Add(Band);
			BandCounts.FindOrAdd(Band, 0)++;

			// At BandEligQ=1.7: Brittle(0.0), Standard(1.0), Tempered(1.5) eligible
			ASSERT_THAT(IsTrue(Band <= 2,
				TEXT("At quality 1.7 only bands 0-2 should be eligible")));
		}

		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Brittle should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(1), TEXT("Standard should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(2), TEXT("Tempered should appear")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(3), TEXT("Masterwork band should NOT appear")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  MixedTier (Fine+Masterwork): AvgQ=1.7, Brittle=%d, Standard=%d, Tempered=%d, Masterwork=%d"),
			BandCounts.FindRef(0), BandCounts.FindRef(1), BandCounts.FindRef(2), BandCounts.FindRef(3));
	}

	/**
	 * Scenario: Two Legendary iron ingots.
	 * Legendary=3.0 each → AverageQuality = 3.0
	 * BandEligibilityQuality=3.0 → All four bands eligible including Masterwork (MinQ=2.5).
	 *
	 * Shows: top-tier materials unlock the best quality bands.
	 * Masterwork band has QualityWeightBias=1.0 so it gains weight at high quality.
	 */
	TEST_METHOD(LegendaryIngredients_UnlocksMasterworkBand)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		FArcItemData LegIron1;
		LegIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		LegIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Legendary);

		FArcItemData LegIron2;
		LegIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		LegIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Legendary);

		TArray<FArcItemData*> Items = { &LegIron1, &LegIron2 };

		ASSERT_THAT(IsNear(3.0f, TierTable->EvaluateQuality(LegIron1.GetItemAggregatedTags()), 0.001f));

		TSet<int32> ObservedBands;
		TMap<int32, int32> BandCounts;

		for (int32 Run = 0; Run < 200; ++Run)
		{
			auto Result = RealisticQualityHelpers::RunFullCraftPipeline(PropTable, TierTable, Items);

			ASSERT_THAT(IsNear(3.0f, Result.Context.AverageQuality, 0.001f));
			ASSERT_THAT(IsNear(3.0f, Result.BandEligibilityQuality, 0.001f));
			ASSERT_THAT(AreEqual(1, Result.Evaluations.Num()));

			const int32 Band = Result.Evaluations[0].SelectedBandIndex;
			ObservedBands.Add(Band);
			BandCounts.FindOrAdd(Band, 0)++;
		}

		// All 4 bands should be eligible at quality 3.0
		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Brittle should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(1), TEXT("Standard should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(2), TEXT("Tempered should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(3), TEXT("Masterwork should appear")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  LegendaryIngredients: AvgQ=3.0, Brittle=%d, Standard=%d, Tempered=%d, Masterwork=%d"),
			BandCounts.FindRef(0), BandCounts.FindRef(1), BandCounts.FindRef(2), BandCounts.FindRef(3));
	}

	/**
	 * Scenario: Player uses two Common iron (quality 1.0 each, AverageQ=1.0),
	 * but adds TWO EXTRA items (total 4, base expected = 2).
	 *
	 * Table's ExtraIngredientWeightBonus = 0.15
	 * Extra = max(0, 4-2) = 2 → weight bonus from ingredients = 2 * 0.15 = 0.3
	 * Plus ExtraCraftTimeBonus = 0.3, capped at 0.5
	 *
	 * BandEligibilityQuality = AverageQuality = 1.0 (NOT boosted by extras)
	 * BandWeightBonus = 0.3 (ingredients) + 0.3 (time) = 0.6
	 *
	 * Extras do NOT unlock higher bands (Tempered MinQ=1.5 is still NOT eligible),
	 * but they DO increase BandWeightBonus which affects weight formula only.
	 *
	 * Shows: the new "remedy" system where adding more materials and time
	 * increases weight bonus but does not change band eligibility.
	 */
	TEST_METHOD(CommonIngredients_WithRemedies_DoNotUnlockBands_ButIncreaseWeight)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		// 4 Common iron items (2 base + 2 extra)
		FArcItemData CommonIron1, CommonIron2, CommonIron3, CommonIron4;
		CommonIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);
		CommonIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);
		CommonIron3.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron3.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);
		CommonIron4.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron4.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);

		TArray<FArcItemData*> Items = { &CommonIron1, &CommonIron2, &CommonIron3, &CommonIron4 };

		auto Result = RealisticQualityHelpers::RunFullCraftPipeline(
			PropTable, TierTable, Items,
			/*BaseIngredientCount=*/ 2,
			/*ExtraCraftTimeBonus=*/ 0.3f);

		// AverageQuality = (1.0 + 1.0 + 1.0 + 1.0) / 4 = 1.0
		ASSERT_THAT(IsNear(1.0f, Result.Context.AverageQuality, 0.001f));

		// BandEligibilityQuality = AverageQuality = 1.0 (NOT boosted)
		ASSERT_THAT(IsNear(1.0f, Result.BandEligibilityQuality, 0.001f,
			TEXT("BandEligibilityQuality should equal AverageQuality, not boosted by extras")));

		// BandWeightBonus = max(0, 4-2)*0.15 + min(0.3, 0.5) = 0.3 + 0.3 = 0.6
		const float ExpectedWeightBonus = 2.0f * 0.15f + 0.3f;
		ASSERT_THAT(IsNear(ExpectedWeightBonus, Result.BandWeightBonus, 0.001f,
			TEXT("BandWeightBonus should be 0.6 from extra ingredients + time")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  Remedies: AvgQ=%.2f, BandEligQ=%.2f, WeightBonus=%.2f (ingredient=%.2f, time=%.2f)"),
			Result.Context.AverageQuality, Result.BandEligibilityQuality,
			Result.BandWeightBonus, 2.0f * 0.15f, 0.3f);

		// At BandEligibilityQuality=1.0, Tempered band (MinQ=1.5) is NOT eligible
		// Only Brittle (MinQ=0.0) and Standard (MinQ=1.0) are eligible
		TSet<int32> ObservedBands;
		TMap<int32, int32> BandCounts;

		for (int32 Run = 0; Run < 200; ++Run)
		{
			auto RunResult = RealisticQualityHelpers::RunFullCraftPipeline(
				PropTable, TierTable, Items, 2, 0.3f);

			ASSERT_THAT(AreEqual(1, RunResult.Evaluations.Num()));
			const int32 Band = RunResult.Evaluations[0].SelectedBandIndex;
			ObservedBands.Add(Band);
			BandCounts.FindOrAdd(Band, 0)++;
		}

		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Brittle should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(1), TEXT("Standard should appear")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(2),
			TEXT("Tempered should NOT appear — extras affect weight, not band eligibility (MinQ=1.5 > BandEligQ=1.0)")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(3),
			TEXT("Masterwork should NOT appear — BandEligQ=1.0 < MinQ=2.5")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  Remedies weight only: Brittle=%d, Standard=%d, Tempered=%d, Masterwork=%d"),
			BandCounts.FindRef(0), BandCounts.FindRef(1), BandCounts.FindRef(2), BandCounts.FindRef(3));
	}

	/**
	 * Scenario: Same Common materials WITHOUT remedies (baseline test for comparison).
	 * Common x 2, base=2, no extra time → BandEligibilityQuality = 1.0
	 * Tempered band (MinQ=1.5) should NEVER appear.
	 *
	 * Contrast with CommonIngredients_WithRemedies test above.
	 */
	TEST_METHOD(CommonIngredients_WithoutRemedies_TemperedNeverAppears)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		FArcItemData CommonIron1, CommonIron2;
		CommonIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);
		CommonIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);

		TArray<FArcItemData*> Items = { &CommonIron1, &CommonIron2 };

		TSet<int32> ObservedBands;
		for (int32 Run = 0; Run < 100; ++Run)
		{
			auto Result = RealisticQualityHelpers::RunFullCraftPipeline(
				PropTable, TierTable, Items,
				/*BaseIngredientCount=*/ 2,
				/*ExtraCraftTimeBonus=*/ 0.0f);

			ASSERT_THAT(IsNear(1.0f, Result.BandEligibilityQuality, 0.001f));

			const int32 Band = Result.Evaluations[0].SelectedBandIndex;
			ObservedBands.Add(Band);
		}

		ASSERT_THAT(IsFalse(ObservedBands.Contains(2),
			TEXT("Without remedies, Tempered band should never appear at quality 1.0")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(3),
			TEXT("Masterwork should never appear at quality 1.0")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  No remedies: only bands %s observed over 100 runs"),
			ObservedBands.Contains(0) && ObservedBands.Contains(1) ? TEXT("0,1") : TEXT("subset"));
	}

	/**
	 * Scenario: 3-ingredient weapon with mixed tiers and one extra.
	 * Recipe expects 2 ingredients, player adds 3 (Fine, Common, Masterwork).
	 *
	 * Quality mults: Fine=1.4, Common=1.0, Masterwork=2.0
	 * AverageQuality = (1.4 + 1.0 + 2.0) / 3 = 1.467
	 * BandEligibilityQuality = AverageQuality = 1.467 (NOT boosted by extras)
	 * BandWeightBonus = max(0, 3-2) * 0.15 = 0.15
	 *
	 * BandEligibilityQuality 1.467 < Tempered MinQ 1.5, so Tempered is NOT eligible.
	 * Extra ingredient only contributes to BandWeightBonus.
	 * Shows realistic 3-ingredient mixed-quality craft with new behavior.
	 */
	TEST_METHOD(ThreeIngredients_MixedTiers_OneExtra_RealisticCraft)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		FArcItemData FineIron;
		FineIron.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		FineIron.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Fine);

		FArcItemData CommonIron;
		CommonIron.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);

		FArcItemData MasterworkIron;
		MasterworkIron.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		MasterworkIron.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Masterwork);

		TArray<FArcItemData*> Items = { &FineIron, &CommonIron, &MasterworkIron };

		// Single run to verify math
		auto Result = RealisticQualityHelpers::RunFullCraftPipeline(
			PropTable, TierTable, Items,
			/*BaseIngredientCount=*/ 2);

		const float ExpectedAvgQ = (1.4f + 1.0f + 2.0f) / 3.0f;
		ASSERT_THAT(IsNear(ExpectedAvgQ, Result.Context.AverageQuality, 0.01f,
			TEXT("Average quality should be mean of individual quality mults")));

		// BandEligibilityQuality = AverageQuality (NOT boosted by extras)
		ASSERT_THAT(IsNear(ExpectedAvgQ, Result.BandEligibilityQuality, 0.01f,
			TEXT("BandEligibilityQuality should equal AverageQuality, not boosted")));

		// BandWeightBonus = 1 extra * 0.15 = 0.15
		const float ExpectedWeightBonus = 1.0f * 0.15f;
		ASSERT_THAT(IsNear(ExpectedWeightBonus, Result.BandWeightBonus, 0.01f,
			TEXT("BandWeightBonus should be 0.15 from 1 extra ingredient")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  3-Ingredient craft: Fine(1.4) + Common(1.0) + Masterwork(2.0)"));
		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("    AvgQ=%.3f, BandEligQ=%.3f, WeightBonus=%.3f"),
			Result.Context.AverageQuality, Result.BandEligibilityQuality, Result.BandWeightBonus);

		// Run many times to see band distribution
		TSet<int32> ObservedBands;
		TMap<int32, int32> BandCounts;

		for (int32 Run = 0; Run < 200; ++Run)
		{
			auto RunResult = RealisticQualityHelpers::RunFullCraftPipeline(
				PropTable, TierTable, Items, 2);
			ASSERT_THAT(AreEqual(1, RunResult.Evaluations.Num()));

			const int32 Band = RunResult.Evaluations[0].SelectedBandIndex;
			ObservedBands.Add(Band);
			BandCounts.FindOrAdd(Band, 0)++;
		}

		// BandEligibilityQuality ~= 1.467, so Brittle(0.0) and Standard(1.0) eligible
		// Tempered(1.5) is NOT eligible because 1.467 < 1.5
		ASSERT_THAT(IsTrue(ObservedBands.Contains(0), TEXT("Brittle should appear")));
		ASSERT_THAT(IsTrue(ObservedBands.Contains(1), TEXT("Standard should appear")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(2),
			TEXT("Tempered should NOT appear — BandEligQ 1.467 < MinQ 1.5")));
		ASSERT_THAT(IsFalse(ObservedBands.Contains(3),
			TEXT("Masterwork should NOT appear — BandEligQ 1.467 < MinQ 2.5")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("    Bands: Brittle=%d, Standard=%d, Tempered=%d, Masterwork=%d"),
			BandCounts.FindRef(0), BandCounts.FindRef(1), BandCounts.FindRef(2), BandCounts.FindRef(3));
	}

	/**
	 * Scenario: TierTable evaluation precedence.
	 * An item with BOTH Fine and Masterwork tier tags should evaluate to the BEST one.
	 * UArcQualityTierTable::FindBestTierTag returns the highest TierValue match.
	 */
	TEST_METHOD(ItemWithMultipleTierTags_BestTierWins)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();

		// Item has both Fine and Masterwork tier tags
		FArcItemData MultiTierItem;
		MultiTierItem.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		MultiTierItem.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Fine);
		MultiTierItem.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Masterwork);

		// FindBestTierTag should find Masterwork (TierValue=3) over Fine (TierValue=2)
		const FGameplayTag BestTag = TierTable->FindBestTierTag(MultiTierItem.GetItemAggregatedTags());
		ASSERT_THAT(IsTrue(BestTag == TAG_IntTest_Material_Tier_Masterwork,
			TEXT("Masterwork (TierValue=3) should win over Fine (TierValue=2)")));

		// Quality should be Masterwork's 2.0, not Fine's 1.4
		const float Quality = TierTable->EvaluateQuality(MultiTierItem.GetItemAggregatedTags());
		ASSERT_THAT(IsNear(2.0f, Quality, 0.001f,
			TEXT("Quality should be Masterwork's 2.0")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  MultiTier: Best=%s, Quality=%.2f"), *BestTag.ToString(), Quality);
	}

	/**
	 * Scenario: Item with NO tier tag — defaults to quality 1.0.
	 * This simulates items that don't participate in the quality system
	 * (e.g., a generic crafting catalyst with no material tier).
	 */
	TEST_METHOD(ItemWithNoTierTag_DefaultsToBaselineQuality)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();

		FArcItemData UntieredItem;
		UntieredItem.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		// No tier tag added

		const float Quality = TierTable->EvaluateQuality(UntieredItem.GetItemAggregatedTags());
		ASSERT_THAT(IsNear(1.0f, Quality, 0.001f,
			TEXT("No matching tier tag should return default quality 1.0")));

		// Craft with one tiered and one untiered item
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		FArcItemData MasterworkIron;
		MasterworkIron.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		MasterworkIron.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Masterwork);

		TArray<FArcItemData*> Items = { &MasterworkIron, &UntieredItem };

		auto Result = RealisticQualityHelpers::RunFullCraftPipeline(PropTable, TierTable, Items);

		// AverageQuality = (2.0 + 1.0) / 2 = 1.5
		ASSERT_THAT(IsNear(1.5f, Result.Context.AverageQuality, 0.001f,
			TEXT("Masterwork(2.0) + untiered(1.0) → average should be 1.5")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  Untiered: Masterwork(2.0) + NoTier(1.0) = AvgQ=%.2f, BandEligQ=%.2f"),
			Result.Context.AverageQuality, Result.BandEligibilityQuality);
	}

	/**
	 * Scenario: Full end-to-end with stat value verification.
	 * Two Masterwork iron → AvgQ=2.0, BandEligibilityQuality=2.0
	 *
	 * If Tempered band (BaseValue=40, QualityScaling=1.0) is selected:
	 *   FinalStat = 40 * (1 + (2.0-1.0) * 1.0) = 40 * 2.0 = 80.0
	 *
	 * If Standard band (BaseValue=15, QualityScaling=1.0) is selected:
	 *   FinalStat = 15 * (1 + (2.0-1.0) * 1.0) = 15 * 2.0 = 30.0
	 *
	 * Verifies that quality both determines which band is accessible AND
	 * scales the resulting stat value within that band.
	 */
	TEST_METHOD(MasterworkIngredients_StatScaledByQuality)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();

		FArcItemData MWIron1, MWIron2;
		MWIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		MWIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Masterwork);
		MWIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		MWIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Masterwork);

		TArray<FArcItemData*> Items = { &MWIron1, &MWIron2 };

		// Expected stat values at BandEligQ=2.0 for each band:
		// Brittle:    5  * (1 + (2-1)*1) = 5  * 2 = 10
		// Standard:   15 * (1 + (2-1)*1) = 15 * 2 = 30
		// Tempered:   40 * (1 + (2-1)*1) = 40 * 2 = 80
		// Masterwork: NOT eligible (MinQ=2.5 > BandEligQ=2.0)
		const TMap<int32, float> ExpectedStatByBand = {
			{0, 10.0f}, {1, 30.0f}, {2, 80.0f}
		};

		TMap<int32, int32> BandCounts;

		for (int32 Run = 0; Run < 150; ++Run)
		{
			auto Result = RealisticQualityHelpers::RunFullCraftPipeline(PropTable, TierTable, Items);

			ASSERT_THAT(IsNear(2.0f, Result.BandEligibilityQuality, 0.001f));
			ASSERT_THAT(AreEqual(1, Result.Evaluations.Num()));

			const int32 Band = Result.Evaluations[0].SelectedBandIndex;
			BandCounts.FindOrAdd(Band, 0)++;

			const FArcItemFragment_ItemStats* Stats = Result.OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
			ASSERT_THAT(IsNotNull(Stats));
			ASSERT_THAT(AreEqual(1, Stats->DefaultStats.Num()));

			// Verify the stat value matches the expected for this band
			const float* ExpectedStat = ExpectedStatByBand.Find(Band);
			ASSERT_THAT(IsNotNull(ExpectedStat,
				TEXT("Band should be in expected set (0-2)")));
			ASSERT_THAT(IsNear(*ExpectedStat, Stats->DefaultStats[0].Value.GetValue(), 0.01f,
				TEXT("Stat should be quality-scaled correctly for the selected band")));
		}

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  Masterwork stat scaling: Brittle(10)=%d, Standard(30)=%d, Tempered(80)=%d, Masterwork(N/A)=%d"),
			BandCounts.FindRef(0), BandCounts.FindRef(1), BandCounts.FindRef(2), BandCounts.FindRef(3));
	}

	/**
	 * Scenario: Demonstrating that the ExtraCraftTimeBonus weight bonus is capped.
	 * Table has ExtraTimeWeightBonusCap = 0.5.
	 * Even if we pass ExtraCraftTimeBonus = 5.0, the weight bonus from time is capped at 0.5.
	 *
	 * BandEligibilityQuality = AverageQuality = 1.0 (unchanged by extras)
	 * BandWeightBonus = 0.0 (no extra ingredients) + min(5.0, 0.5) = 0.5
	 */
	TEST_METHOD(ExtraCraftTimeBonus_CappedByTable)
	{
		UArcQualityTierTable* TierTable = RealisticQualityHelpers::CreateTierTable();
		UArcMaterialPropertyTable* PropTable = RealisticQualityHelpers::CreateWeaponTable();
		// Table has ExtraTimeWeightBonusCap = 0.5

		FArcItemData CommonIron1, CommonIron2;
		CommonIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron1.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);
		CommonIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Resource_Metal_Iron);
		CommonIron2.ItemAggregatedTags.AddTag(TAG_IntTest_Material_Tier_Common);

		TArray<FArcItemData*> Items = { &CommonIron1, &CommonIron2 };

		// Pass a huge time bonus — should be capped
		auto Result = RealisticQualityHelpers::RunFullCraftPipeline(
			PropTable, TierTable, Items,
			/*BaseIngredientCount=*/ 2,
			/*ExtraCraftTimeBonus=*/ 5.0f);

		// BandEligibilityQuality = AverageQuality = 1.0 (not affected by extras)
		ASSERT_THAT(IsNear(1.0f, Result.BandEligibilityQuality, 0.001f,
			TEXT("BandEligibilityQuality should equal AverageQuality (1.0), not affected by time bonus")));

		// BandWeightBonus = 0.0 (no extra ingredients) + min(5.0, 0.5) = 0.5
		ASSERT_THAT(IsNear(0.5f, Result.BandWeightBonus, 0.001f,
			TEXT("BandWeightBonus from time should be capped at table's 0.5")));

		UE_LOG(LogArcCraftIntegrationTest, Log,
			TEXT("  TimeBonus cap: requested=5.0, cap=0.5, AvgQ=%.2f, BandEligQ=%.2f, WeightBonus=%.2f"),
			Result.Context.AverageQuality, Result.BandEligibilityQuality, Result.BandWeightBonus);
	}
};
