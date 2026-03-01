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
#include "Abilities/GameplayAbility.h"
#include "ArcCraft/Recipe/ArcCraftSlotResolver.h"
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"
#include "Items/Fragments/ArcItemFragment_GrantedAbilities.h"
#include "Items/Fragments/ArcItemFragment_GrantedGameplayEffects.h"

// Tags for output modifier tests
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OutTest_Resource_Metal, "Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OutTest_Resource_Gem, "Resource.Gem");

namespace ArcRecipeOutputModifierTestHelpers
{
	/** Create a transient UArcItemDefinition. */
	UArcItemDefinition* CreateTransientItemDef(const FName& Name)
	{
		UArcItemDefinition* Def = NewObject<UArcItemDefinition>(
			GetTransientPackage(), Name, RF_Transient);
		Def->RegenerateItemId();
		return Def;
	}

	/** Create an FArcItemSpec backed by a transient item definition. */
	FArcItemSpec MakeTestItemSpec(const UArcItemDefinition* ItemDef)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinitionAsset(ItemDef);
		Spec.SetItemDefinition(ItemDef->GetPrimaryAssetId());
		return Spec;
	}

	/** Apply a single modifier result to an item spec. */
	void ApplyResult(FArcItemSpec& OutSpec, const FArcCraftModifierResult& Result)
	{
		switch (Result.Type)
		{
		case EArcCraftModifierResultType::Stat:
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
			break;
		case EArcCraftModifierResultType::Ability:
			if (Result.Ability.GrantedAbility)
			{
				FArcItemFragment_GrantedAbilities* Frag = new FArcItemFragment_GrantedAbilities();
				Frag->Abilities.Add(Result.Ability);
				OutSpec.AddInstanceData(Frag);
			}
			break;
		case EArcCraftModifierResultType::Effect:
			if (Result.Effect)
			{
				FArcItemFragment_GrantedGameplayEffects* Frag = new FArcItemFragment_GrantedGameplayEffects();
				Frag->Effects.Add(Result.Effect);
				OutSpec.AddInstanceData(Frag);
			}
			break;
		default:
			break;
		}
	}

	/** Evaluate a modifier and apply all resulting pending modifiers to OutSpec. */
	TArray<FArcCraftPendingModifier> EvaluateAndApply(
		const FArcRecipeOutputModifier& Modifier,
		FArcItemSpec& OutSpec,
		const TArray<FArcItemSpec>& Ingredients,
		const TArray<float>& QualityMults,
		float AverageQuality)
	{
		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(
			Ingredients, QualityMults, AverageQuality);
		for (const FArcCraftPendingModifier& Pending : Results)
		{
			if (Pending.Result.Type != EArcCraftModifierResultType::None)
			{
				ApplyResult(OutSpec, Pending.Result);
			}
		}
		return Results;
	}
}

// ===================================================================
// FArcRecipeOutputModifier_Stats tests
// ===================================================================

TEST_CLASS(RecipeOutput_StatModifier, "ArcCraft.Recipe.OutputModifier.Stats")
{
	TEST_METHOD(Evaluate_AddsStats)
	{
		FArcRecipeOutputModifier_Stats Modifier;

		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.QualityScalingFactor = 1.0f;

		FArcItemSpec OutSpec;
		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		ArcRecipeOutputModifierTestHelpers::EvaluateAndApply(Modifier, OutSpec, Ingredients, QualityMults, 1.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment, TEXT("Should have created stats fragment")));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num(), TEXT("Should have one stat")));
		ASSERT_THAT(IsNear(10.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Stat value at quality 1.0 should be base value")));
	}

	TEST_METHOD(Evaluate_QualityScalesStats)
	{
		FArcRecipeOutputModifier_Stats Modifier;

		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.QualityScalingFactor = 1.0f;

		FArcItemSpec OutSpec;
		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		// Quality 2.0: scale = 1.0 + (2.0-1.0)*1.0 = 2.0 => 10.0*2.0 = 20.0
		ArcRecipeOutputModifierTestHelpers::EvaluateAndApply(Modifier, OutSpec, Ingredients, QualityMults, 2.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(IsNear(20.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Quality 2.0 with scale 1.0 should double the base stat")));
	}

	TEST_METHOD(Evaluate_ZeroScaling_IgnoresQuality)
	{
		FArcRecipeOutputModifier_Stats Modifier;

		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.QualityScalingFactor = 0.0f;

		FArcItemSpec OutSpec;
		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		// Quality 5.0 but scaling 0.0 => scale = 1.0 + (5.0-1.0)*0.0 = 1.0
		ArcRecipeOutputModifierTestHelpers::EvaluateAndApply(Modifier, OutSpec, Ingredients, QualityMults, 5.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(IsNear(10.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Zero scaling should ignore quality")));
	}

	TEST_METHOD(Evaluate_DefaultStat_ProducesFragment)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		// BaseStat is default-initialized (value 0)

		FArcItemSpec OutSpec;
		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		ArcRecipeOutputModifierTestHelpers::EvaluateAndApply(Modifier, OutSpec, Ingredients, QualityMults, 1.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment, TEXT("Default stat should still produce a fragment")));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num(), TEXT("Should have one stat")));
		ASSERT_THAT(IsNear(0.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Default stat value should be 0.0")));
	}

	TEST_METHOD(Evaluate_MultipleCalls_Accumulate)
	{
		FArcItemSpec OutSpec;
		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		FArcRecipeOutputModifier_Stats ModifierA;
		FArcItemAttributeStat StatA;
		StatA.SetValue(10.0f);
		ModifierA.BaseStat = StatA;
		ModifierA.QualityScalingFactor = 1.0f;

		FArcRecipeOutputModifier_Stats ModifierB;
		FArcItemAttributeStat StatB;
		StatB.SetValue(5.0f);
		ModifierB.BaseStat = StatB;
		ModifierB.QualityScalingFactor = 1.0f;

		ArcRecipeOutputModifierTestHelpers::EvaluateAndApply(ModifierA, OutSpec, Ingredients, QualityMults, 1.0f);
		ArcRecipeOutputModifierTestHelpers::EvaluateAndApply(ModifierB, OutSpec, Ingredients, QualityMults, 1.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(2, StatsFragment->DefaultStats.Num(),
			TEXT("Two modifiers should add two stats")));
	}
};

// ===================================================================
// FArcRecipeOutputModifier_Abilities tests
// ===================================================================

TEST_CLASS(RecipeOutput_AbilityModifier, "ArcCraft.Recipe.OutputModifier.Abilities")
{
	TEST_METHOD(NoAbility_ProducesEmptyResult)
	{
		FArcRecipeOutputModifier_Abilities Modifier;
		// No ability set

		FArcItemSpec OutSpec;
		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		ArcRecipeOutputModifierTestHelpers::EvaluateAndApply(Modifier, OutSpec, Ingredients, QualityMults, 1.0f);
		// Should not crash
	}

	TEST_METHOD(TriggerTags_NotPresent_EvaluateReturnsEmpty)
	{
		FArcRecipeOutputModifier_Abilities Modifier;
		Modifier.TriggerTags.AddTag(TAG_OutTest_Resource_Gem);

		UArcItemDefinition* Def = ArcRecipeOutputModifierTestHelpers::CreateTransientItemDef(TEXT("TestItem_AbilTrigger"));
		FArcItemSpec SpecA = ArcRecipeOutputModifierTestHelpers::MakeTestItemSpec(Def);

		TArray<FArcItemSpec> Ingredients = { SpecA };
		TArray<float> QualityMults = { 1.0f };

		// Should return empty because the item doesn't have Gem tags
		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 1.0f);
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Should produce no pending modifiers when trigger tags don't match")));
	}
};

// ===================================================================
// FArcRecipeOutputModifier_Effects tests
// ===================================================================

TEST_CLASS(RecipeOutput_EffectModifier, "ArcCraft.Recipe.OutputModifier.Effects")
{
	TEST_METHOD(BelowMinQuality_EvaluateReturnsEmpty)
	{
		FArcRecipeOutputModifier_Effects Modifier;
		Modifier.TriggerTags.AddTag(TAG_OutTest_Resource_Metal);
		Modifier.MinQualityThreshold = 3.0f;

		UArcItemDefinition* Def = ArcRecipeOutputModifierTestHelpers::CreateTransientItemDef(TEXT("TestItem_EffectQual"));
		FArcItemSpec SpecA = ArcRecipeOutputModifierTestHelpers::MakeTestItemSpec(Def);

		TArray<FArcItemSpec> Ingredients = { SpecA };
		TArray<float> QualityMults = { 1.0f };

		// Quality 1.0 < threshold 3.0 => should produce no results
		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 1.0f);
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Should produce no pending modifiers when quality below threshold")));
	}

	TEST_METHOD(NoEffect_ProducesResult)
	{
		FArcRecipeOutputModifier_Effects Modifier;
		// No effect set, no quality/tag gates

		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		// Should still produce a pending modifier (effect validity checked at application time)
		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 1.0f);
		ASSERT_THAT(AreEqual(1, Results.Num(),
			TEXT("Should produce a pending modifier even without effect set")));
	}
};

// Tags for Evaluate tests
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_OutTest_Slot_Offense, "Modifier.Offense");

// ===================================================================
// FArcRecipeOutputModifier::Evaluate() base tests
// ===================================================================

TEST_CLASS(RecipeOutput_BaseEvaluate, "ArcCraft.Recipe.OutputModifier.Evaluate.Base")
{
	TEST_METHOD(BaseEvaluate_ProducesOnePendingModifier)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.QualityScalingFactor = 1.0f;

		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 1.0f);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Base Evaluate should produce exactly 1 pending modifier")));
	}

	TEST_METHOD(BaseEvaluate_SlotTagPropagated)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.SlotTag = TAG_OutTest_Slot_Offense;

		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 2.0f);

		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].SlotTag == TAG_OutTest_Slot_Offense,
			TEXT("Pending modifier should carry the modifier's SlotTag")));
	}

	TEST_METHOD(BaseEvaluate_EmptySlotTag_Unslotted)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(5.0f);
		Modifier.BaseStat = Stat;
		// SlotTag not set — empty by default

		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 1.0f);

		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsFalse(Results[0].SlotTag.IsValid(),
			TEXT("Unset SlotTag should produce invalid/empty tag (unslotted)")));
	}

	TEST_METHOD(BaseEvaluate_EffectiveWeight_IncorporatesQualityAndWeight)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.Weight = 2.0f;
		Modifier.QualityScalingFactor = 1.0f;

		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		const float TestQuality = 3.0f;
		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, TestQuality);

		// EffectiveWeight = Weight * (1.0 + (Quality - 1.0) * ScalingFactor)
		// = 2.0 * (1.0 + (3.0 - 1.0) * 1.0) = 2.0 * 3.0 = 6.0
		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsNear(6.0f, Results[0].EffectiveWeight, 0.001f,
			TEXT("EffectiveWeight should be Weight * (1 + (Quality-1) * ScalingFactor)")));
	}

	TEST_METHOD(BaseEvaluate_DefaultWeight_AtQuality1_EqualsWeight)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;

		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 1.0f);

		// At Quality 1.0: EffectiveWeight = 1.0 * (1.0 + 0.0 * 1.0) = 1.0
		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsNear(1.0f, Results[0].EffectiveWeight, 0.001f,
			TEXT("At quality 1.0, EffectiveWeight should equal Weight")));
	}

	TEST_METHOD(BaseEvaluate_ResultProducesCorrectValue)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.QualityScalingFactor = 1.0f;

		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 2.0f);

		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].Result.Type == EArcCraftModifierResultType::Stat,
			TEXT("Result should be a Stat type")));

		// Verify the pre-scaled stat value: 10.0 * (1.0 + (2.0-1.0)*1.0) = 20.0
		ASSERT_THAT(IsNear(20.0f, Results[0].Result.Stat.Value.GetValue(), 0.001f,
			TEXT("Result should carry correct quality-scaled stat value")));

		// Also verify it applies correctly
		FArcItemSpec OutSpec;
		ArcRecipeOutputModifierTestHelpers::ApplyResult(OutSpec, Results[0].Result);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment, TEXT("Applying result should create stats fragment")));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num()));
		ASSERT_THAT(IsNear(20.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Applied result should produce correct quality-scaled stat")));
	}

	TEST_METHOD(BaseEvaluate_IneligibleQuality_ReturnsEmpty)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.MinQualityThreshold = 5.0f;

		TArray<FArcItemSpec> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 2.0f);
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Should produce no pending modifiers when quality below threshold")));
	}

	TEST_METHOD(BaseEvaluate_IneligibleTags_ReturnsEmpty)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStat = Stat;
		Modifier.TriggerTags.AddTag(TAG_OutTest_Resource_Gem);

		UArcItemDefinition* Def = ArcRecipeOutputModifierTestHelpers::CreateTransientItemDef(TEXT("TestItem_BaseEvalTags"));
		FArcItemSpec SpecA = ArcRecipeOutputModifierTestHelpers::MakeTestItemSpec(Def);

		TArray<FArcItemSpec> Ingredients = { SpecA };
		TArray<float> QualityMults = { 1.0f };

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(Ingredients, QualityMults, 1.0f);
		ASSERT_THAT(AreEqual(0, Results.Num(),
			TEXT("Should produce no pending modifiers when trigger tags don't match")));
	}
};
