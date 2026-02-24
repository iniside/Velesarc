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
#include "ArcCraft/Recipe/ArcRecipeOutput.h"
#include "Items/ArcItemData.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_ItemStats.h"

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

	/** Create an FArcItemData backed by a transient item definition. */
	TSharedPtr<FArcItemData> MakeTestItem(const UArcItemDefinition* ItemDef)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinitionAsset(ItemDef);
		return FArcItemData::NewFromSpec(Spec);
	}
}

// ===================================================================
// FArcRecipeOutputModifier_Stats tests
// ===================================================================

TEST_CLASS(RecipeOutput_StatModifier, "ArcCraft.Recipe.OutputModifier.Stats")
{
	TEST_METHOD(ApplyToOutput_AddsStats)
	{
		FArcRecipeOutputModifier_Stats Modifier;

		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStats.Add(Stat);
		Modifier.QualityScalingFactor = 1.0f;

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment, TEXT("Should have created stats fragment")));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num(), TEXT("Should have one stat")));
		ASSERT_THAT(IsNear(10.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Stat value at quality 1.0 should be base value")));
	}

	TEST_METHOD(ApplyToOutput_QualityScalesStats)
	{
		FArcRecipeOutputModifier_Stats Modifier;

		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStats.Add(Stat);
		Modifier.QualityScalingFactor = 1.0f;

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		// Quality 2.0: scale = 1.0 + (2.0-1.0)*1.0 = 2.0 => 10.0*2.0 = 20.0
		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 2.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(IsNear(20.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Quality 2.0 with scale 1.0 should double the base stat")));
	}

	TEST_METHOD(ApplyToOutput_ZeroScaling_IgnoresQuality)
	{
		FArcRecipeOutputModifier_Stats Modifier;

		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStats.Add(Stat);
		Modifier.QualityScalingFactor = 0.0f;

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		// Quality 5.0 but scaling 0.0 => scale = 1.0 + (5.0-1.0)*0.0 = 1.0
		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 5.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(IsNear(10.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Zero scaling should ignore quality")));
	}

	TEST_METHOD(ApplyToOutput_MultipleStats)
	{
		FArcRecipeOutputModifier_Stats Modifier;

		FArcItemAttributeStat StatA;
		StatA.SetValue(10.0f);
		FArcItemAttributeStat StatB;
		StatB.SetValue(20.0f);
		Modifier.BaseStats.Add(StatA);
		Modifier.BaseStats.Add(StatB);
		Modifier.QualityScalingFactor = 1.0f;

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment));
		ASSERT_THAT(AreEqual(2, StatsFragment->DefaultStats.Num(), TEXT("Should have two stats")));
	}

	TEST_METHOD(ApplyToOutput_EmptyStats_NoFragment)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		// No stats added

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);

		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNull(StatsFragment, TEXT("Empty stats should not create fragment")));
	}

	TEST_METHOD(ApplyToOutput_MultipleCalls_Accumulate)
	{
		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcRecipeOutputModifier_Stats ModifierA;
		FArcItemAttributeStat StatA;
		StatA.SetValue(10.0f);
		ModifierA.BaseStats.Add(StatA);
		ModifierA.QualityScalingFactor = 1.0f;

		FArcRecipeOutputModifier_Stats ModifierB;
		FArcItemAttributeStat StatB;
		StatB.SetValue(5.0f);
		ModifierB.BaseStats.Add(StatB);
		ModifierB.QualityScalingFactor = 1.0f;

		ModifierA.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);
		ModifierB.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);

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
	TEST_METHOD(NoTriggerTags_DoesNothing)
	{
		FArcRecipeOutputModifier_Abilities Modifier;
		// Empty trigger tags

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);
		// Should not crash and not modify OutSpec
		// (We can't easily check if no ability fragment was added without knowing the type,
		//  but at least verify no crash.)
	}

	TEST_METHOD(TriggerTags_NotPresent_DoesNothing)
	{
		FArcRecipeOutputModifier_Abilities Modifier;
		Modifier.TriggerTags.AddTag(TAG_OutTest_Resource_Gem);
		// Note: no abilities to grant, but we're testing the tag check here

		UArcItemDefinition* Def = ArcRecipeOutputModifierTestHelpers::CreateTransientItemDef(TEXT("TestItem_AbilTrigger"));
		TSharedPtr<FArcItemData> ItemA = ArcRecipeOutputModifierTestHelpers::MakeTestItem(Def);
		ItemA->ItemAggregatedTags.AddTag(TAG_OutTest_Resource_Metal);

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients = { ItemA.Get() };
		TArray<float> QualityMults = { 1.0f };

		// Should return early because Metal != Gem
		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);
		// Verify no crash — the actual ability granting is harder to test without GAS setup
	}
};

// ===================================================================
// FArcRecipeOutputModifier_Effects tests
// ===================================================================

TEST_CLASS(RecipeOutput_EffectModifier, "ArcCraft.Recipe.OutputModifier.Effects")
{
	TEST_METHOD(BelowMinQuality_DoesNothing)
	{
		FArcRecipeOutputModifier_Effects Modifier;
		Modifier.TriggerTags.AddTag(TAG_OutTest_Resource_Metal);
		Modifier.MinQualityThreshold = 3.0f;
		// No effects to grant, just testing the quality gate

		UArcItemDefinition* Def = ArcRecipeOutputModifierTestHelpers::CreateTransientItemDef(TEXT("TestItem_EffectQual"));
		TSharedPtr<FArcItemData> ItemA = ArcRecipeOutputModifierTestHelpers::MakeTestItem(Def);
		ItemA->ItemAggregatedTags.AddTag(TAG_OutTest_Resource_Metal);

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients = { ItemA.Get() };
		TArray<float> QualityMults = { 1.0f };

		// Quality 1.0 < threshold 3.0 => should not apply
		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);
		// Verify no crash — the actual effect granting requires GE setup
	}

	TEST_METHOD(EmptyTriggerTags_DoesNothing)
	{
		FArcRecipeOutputModifier_Effects Modifier;
		// Empty trigger tags

		FArcItemSpec OutSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		Modifier.ApplyToOutput(OutSpec, Ingredients, QualityMults, 1.0f);
		// Verify no crash
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
		Modifier.BaseStats.Add(Stat);
		Modifier.QualityScalingFactor = 1.0f;

		FArcItemSpec BaseSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(BaseSpec, Ingredients, QualityMults, 1.0f);

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Base Evaluate should produce exactly 1 pending modifier")));
	}

	TEST_METHOD(BaseEvaluate_SlotTagPropagated)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStats.Add(Stat);
		Modifier.SlotTag = TAG_OutTest_Slot_Offense;

		FArcItemSpec BaseSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(BaseSpec, Ingredients, QualityMults, 2.0f);

		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].SlotTag == TAG_OutTest_Slot_Offense,
			TEXT("Pending modifier should carry the modifier's SlotTag")));
	}

	TEST_METHOD(BaseEvaluate_EmptySlotTag_Unslotted)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(5.0f);
		Modifier.BaseStats.Add(Stat);
		// SlotTag not set — empty by default

		FArcItemSpec BaseSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(BaseSpec, Ingredients, QualityMults, 1.0f);

		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsFalse(Results[0].SlotTag.IsValid(),
			TEXT("Unset SlotTag should produce invalid/empty tag (unslotted)")));
	}

	TEST_METHOD(BaseEvaluate_EffectiveWeightEqualsAverageQuality)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStats.Add(Stat);

		FArcItemSpec BaseSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		const float TestQuality = 3.5f;
		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(BaseSpec, Ingredients, QualityMults, TestQuality);

		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsNear(TestQuality, Results[0].EffectiveWeight, 0.001f,
			TEXT("Base Evaluate sets EffectiveWeight = AverageQuality")));
	}

	TEST_METHOD(BaseEvaluate_ApplyFnProducesCorrectResult)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat Stat;
		Stat.SetValue(10.0f);
		Modifier.BaseStats.Add(Stat);
		Modifier.QualityScalingFactor = 1.0f;

		FArcItemSpec BaseSpec;
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(BaseSpec, Ingredients, QualityMults, 2.0f);

		ASSERT_THAT(AreEqual(1, Results.Num()));
		ASSERT_THAT(IsTrue(Results[0].ApplyFn != nullptr, TEXT("ApplyFn should be set")));

		// Execute the deferred apply
		FArcItemSpec OutSpec;
		Results[0].ApplyFn(OutSpec);

		// Verify the stats were applied correctly: 10.0 * (1.0 + (2.0-1.0)*1.0) = 20.0
		const FArcItemFragment_ItemStats* StatsFragment = OutSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		ASSERT_THAT(IsNotNull(StatsFragment, TEXT("ApplyFn should create stats fragment")));
		ASSERT_THAT(AreEqual(1, StatsFragment->DefaultStats.Num()));
		ASSERT_THAT(IsNear(20.0f, StatsFragment->DefaultStats[0].Value.GetValue(), 0.001f,
			TEXT("Deferred apply should produce same result as direct ApplyToOutput")));
	}

	TEST_METHOD(BaseEvaluate_ApplyFnMatchesDirectApply)
	{
		FArcRecipeOutputModifier_Stats Modifier;
		FArcItemAttributeStat StatA;
		StatA.SetValue(10.0f);
		FArcItemAttributeStat StatB;
		StatB.SetValue(25.0f);
		Modifier.BaseStats.Add(StatA);
		Modifier.BaseStats.Add(StatB);
		Modifier.QualityScalingFactor = 0.5f;

		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;
		const float Quality = 3.0f;

		// Direct apply
		FArcItemSpec DirectSpec;
		Modifier.ApplyToOutput(DirectSpec, Ingredients, QualityMults, Quality);

		// Deferred apply via Evaluate
		FArcItemSpec BaseSpec;
		TArray<FArcCraftPendingModifier> Results = Modifier.Evaluate(BaseSpec, Ingredients, QualityMults, Quality);
		FArcItemSpec DeferredSpec;
		Results[0].ApplyFn(DeferredSpec);

		// Compare results
		const FArcItemFragment_ItemStats* DirectStats = DirectSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();
		const FArcItemFragment_ItemStats* DeferredStats = DeferredSpec.FindFragmentMutable<FArcItemFragment_ItemStats>();

		ASSERT_THAT(IsNotNull(DirectStats));
		ASSERT_THAT(IsNotNull(DeferredStats));
		ASSERT_THAT(AreEqual(DirectStats->DefaultStats.Num(), DeferredStats->DefaultStats.Num(),
			TEXT("Same number of stats")));

		for (int32 i = 0; i < DirectStats->DefaultStats.Num(); ++i)
		{
			ASSERT_THAT(IsNear(
				DirectStats->DefaultStats[i].Value.GetValue(),
				DeferredStats->DefaultStats[i].Value.GetValue(),
				0.001f,
				TEXT("Deferred apply must match direct apply")));
		}
	}
};
