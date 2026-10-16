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
#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

// Tags for context tests -- unique names to avoid ODR with evaluator test tags
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Metal, "Test.Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Gem, "Test.Resource.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Wood, "Test.Resource.Wood");

// ===================================================================
// Helper: write an item definition record with optional item tags
// ===================================================================

namespace ArcCraftContextTestHelpers
{
	/**
	 * Write an item definition record and optionally inject an
	 * FArcItemFragment_Tags whose AssetTags (what Build reads per slot) are the given tags.
	 */
	FPrimaryAssetId CreateItemDef(
		FArcItemTestRecordFiles& ItemRecords,
		const FGameplayTagContainer& AssetTags = FGameplayTagContainer())
	{
		FArcItemDefinitionData Def;

		if (AssetTags.Num() > 0)
		{
			FArcItemFragment_Tags TagsFragment;
			TagsFragment.AssetTags = AssetTags;
			ArcItemTestRecords::AddFragment(Def, TagsFragment);
		}

		return ItemRecords.Write(Def);
	}

	/**
	 * Create an FArcItemSpec backed by an item definition record.
	 */
	FArcItemSpec MakeTestItemSpec(
		const FPrimaryAssetId& ItemDef)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinition(ItemDef);
		return Spec;
	}
}

// ===================================================================
// FArcMaterialCraftContext::Build tests
// ===================================================================

TEST_CLASS(MaterialCraft_ContextBuild, "ArcCraft.MaterialCraft.Context")
{
	FArcItemTestRecordFiles ItemRecords;
	TEST_METHOD(Build_PopulatesPerSlotTags)
	{
		// Create item definitions with actual tags so Build() reads them
		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_CtxTest_Resource_Metal);
		const FPrimaryAssetId MetalDef = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords, MetalTags);
		ASSERT_THAT(IsTrue(MetalDef.IsValid(), TEXT("Item record must be written and indexed")));

		FGameplayTagContainer GemTags;
		GemTags.AddTag(TAG_CtxTest_Resource_Gem);
		const FPrimaryAssetId GemDef = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords, GemTags);
		ASSERT_THAT(IsTrue(GemDef.IsValid(), TEXT("Item record must be written and indexed")));

		FArcItemSpec SpecA = ArcCraftContextTestHelpers::MakeTestItemSpec(MetalDef);
		FArcItemSpec SpecB = ArcCraftContextTestHelpers::MakeTestItemSpec(GemDef);

		TArray<FArcItemSpec> Ingredients = { SpecA, SpecB };
		TArray<float> SlotRaritySteps = { 1.0f, 1.2f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.1f, 2, 0.5f);

		ASSERT_THAT(IsTrue(Ctx.PerSlotTags[0].HasTag(TAG_CtxTest_Resource_Metal),
			TEXT("Slot 0 should have Metal tag from item A")));
		ASSERT_THAT(IsTrue(Ctx.PerSlotTags[1].HasTag(TAG_CtxTest_Resource_Gem),
			TEXT("Slot 1 should have Gem tag from item B")));
	}

	TEST_METHOD(Build_SetsRaritySteps)
	{
		const FPrimaryAssetId Def = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec SpecA = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);

		TArray<FArcItemSpec> Ingredients = { SpecA };
		TArray<float> SlotRaritySteps = { 1.5f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.5f);

		ASSERT_THAT(AreEqual(1, Ctx.SlotRaritySteps.Num()));
		ASSERT_THAT(IsNear(1.5f, Ctx.SlotRaritySteps[0], 0.001f));
	}

	TEST_METHOD(Build_SetsIngredientCount)
	{
		const FPrimaryAssetId Def = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec SpecA = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);
		FArcItemSpec SpecB = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);
		FArcItemSpec SpecC = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);

		TArray<FArcItemSpec> Ingredients = { SpecA, SpecB, SpecC };
		TArray<float> SlotRaritySteps = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.0f);

		ASSERT_THAT(AreEqual(3, Ctx.IngredientCount, TEXT("Should track ingredient count")));
	}

	TEST_METHOD(Build_BaseIngredientCount_ZeroUsesActual)
	{
		const FPrimaryAssetId Def = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec SpecA = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);
		FArcItemSpec SpecB = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);

		TArray<FArcItemSpec> Ingredients = { SpecA, SpecB };
		TArray<float> SlotRaritySteps = { 1.0f, 1.0f };

		// BaseIngredientCount = 0 means use actual count
		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.0f, /*BaseIngredientCount=*/ 0);

		ASSERT_THAT(AreEqual(2, Ctx.BaseIngredientCount,
			TEXT("BaseIngredientCount=0 should use actual ingredient count")));
	}

	TEST_METHOD(Build_BaseIngredientCount_ExplicitValue)
	{
		const FPrimaryAssetId Def = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec SpecA = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);
		FArcItemSpec SpecB = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);
		FArcItemSpec SpecC = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);

		TArray<FArcItemSpec> Ingredients = { SpecA, SpecB, SpecC };
		TArray<float> SlotRaritySteps = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.0f, /*BaseIngredientCount=*/ 2);

		ASSERT_THAT(AreEqual(2, Ctx.BaseIngredientCount,
			TEXT("BaseIngredientCount should be set to explicit value")));
		ASSERT_THAT(AreEqual(3, Ctx.IngredientCount,
			TEXT("IngredientCount should be actual count")));
	}

	TEST_METHOD(Build_SetsExtraCraftTimeBonus)
	{
		TArray<FArcItemSpec> Ingredients;
		TArray<float> SlotRaritySteps;

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.0f, 0, /*ExtraCraftTimeBonus=*/ 0.75f);

		ASSERT_THAT(IsNear(0.75f, Ctx.ExtraCraftTimeBonus, 0.001f));
	}

	TEST_METHOD(Build_SetsAverageR)
	{
		TArray<FArcItemSpec> Ingredients;
		TArray<float> SlotRaritySteps;

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 2.3f);

		ASSERT_THAT(IsNear(2.3f, Ctx.AverageR, 0.001f));
		ASSERT_THAT(IsNear(2.3f, Ctx.BandEligibilityR, 0.001f,
			TEXT("BandEligibilityR should initially equal AverageR")));
	}

	TEST_METHOD(Build_SetsBandWeightBonusToZero)
	{
		TArray<FArcItemSpec> Ingredients;
		TArray<float> SlotRaritySteps;

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.5f);

		ASSERT_THAT(IsNear(0.0f, Ctx.BandWeightBonus, 0.001f,
			TEXT("BandWeightBonus should be zero after Build (computed by evaluator)")));
	}

	TEST_METHOD(Build_EmptySpecIngredient_SkippedSafely)
	{
		FGameplayTagContainer WoodTags;
		WoodTags.AddTag(TAG_CtxTest_Resource_Wood);
		const FPrimaryAssetId WoodDef = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords, WoodTags);
		ASSERT_THAT(IsTrue(WoodDef.IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec SpecA = ArcCraftContextTestHelpers::MakeTestItemSpec(WoodDef);

		// Second ingredient is a default-constructed spec with no definition
		FArcItemSpec EmptySpec;
		TArray<FArcItemSpec> Ingredients = { SpecA, EmptySpec };
		TArray<float> SlotRaritySteps = { 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.0f);

		ASSERT_THAT(IsTrue(Ctx.PerSlotTags[0].HasTag(TAG_CtxTest_Resource_Wood),
			TEXT("Should have tags in slot for valid ingredient")));
		ASSERT_THAT(AreEqual(0, Ctx.PerSlotTags[1].Num(),
			TEXT("Empty spec ingredient slot should have empty tags")));
		ASSERT_THAT(AreEqual(1, Ctx.IngredientCount,
			TEXT("IngredientCount counts filled slots only; the empty spec slot is not counted")));
		ASSERT_THAT(AreEqual(2, Ctx.PerSlotTags.Num(),
			TEXT("PerSlotTags array should match ingredient count")));
	}

	TEST_METHOD(Build_MixedFilledAndEmptySlots_CountsFilledOnly)
	{
		const FPrimaryAssetId Def = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = {
			ArcCraftContextTestHelpers::MakeTestItemSpec(Def),
			FArcItemSpec(),
			ArcCraftContextTestHelpers::MakeTestItemSpec(Def),
			FArcItemSpec() };
		TArray<float> SlotRaritySteps = { 1.0f, 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.0f);

		ASSERT_THAT(AreEqual(2, Ctx.IngredientCount,
			TEXT("Two filled slots among four must count 2, not the slot count 4")));
		ASSERT_THAT(AreEqual(4, Ctx.PerSlotTags.Num(),
			TEXT("PerSlotTags keeps one entry per slot, empty ones included")));
	}

	TEST_METHOD(Build_BaseIngredientCountZero_WithEmptySlot_FallsBackToFilledCount)
	{
		const FPrimaryAssetId Def = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));

		TArray<FArcItemSpec> Ingredients = {
			ArcCraftContextTestHelpers::MakeTestItemSpec(Def),
			ArcCraftContextTestHelpers::MakeTestItemSpec(Def),
			FArcItemSpec() };
		TArray<float> SlotRaritySteps = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.0f, /*BaseIngredientCount=*/ 0);

		ASSERT_THAT(AreEqual(2, Ctx.BaseIngredientCount,
			TEXT("BaseIngredientCount 0 falls back to the filled count (2), not the slot count (3)")));
		ASSERT_THAT(AreEqual(Ctx.IngredientCount, Ctx.BaseIngredientCount,
			TEXT("With the fallback there is no extra-ingredient surplus")));
	}

	TEST_METHOD(Build_PerSlotTags_NotMerged)
	{
		// Verify that per-slot tags are kept separate (not merged like the old
		// AggregatedIngredientTags). Two items with the same tag should each
		// have their own copy in their respective slot.
		FArcMaterialCraftContext Ctx;
		Ctx.PerSlotTags.SetNum(2);
		Ctx.PerSlotTags[0].AddTag(TAG_CtxTest_Resource_Metal);
		Ctx.PerSlotTags[1].AddTag(TAG_CtxTest_Resource_Metal);

		ASSERT_THAT(IsTrue(Ctx.PerSlotTags[0].HasTag(TAG_CtxTest_Resource_Metal),
			TEXT("Slot 0 should have Metal tag")));
		ASSERT_THAT(IsTrue(Ctx.PerSlotTags[1].HasTag(TAG_CtxTest_Resource_Metal),
			TEXT("Slot 1 should have Metal tag")));
		// Each slot independently has the tag -- they are not deduplicated across slots
		ASSERT_THAT(AreEqual(1, Ctx.PerSlotTags[0].Num(),
			TEXT("Slot 0 should have exactly one tag")));
		ASSERT_THAT(AreEqual(1, Ctx.PerSlotTags[1].Num(),
			TEXT("Slot 1 should have exactly one tag")));
	}

	TEST_METHOD(Build_AllocatesPerSlotTagsArray)
	{
		const FPrimaryAssetId Def = ArcCraftContextTestHelpers::CreateItemDef(ItemRecords);
		ASSERT_THAT(IsTrue(Def.IsValid(), TEXT("Item record must be written and indexed")));
		FArcItemSpec SpecA = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);
		FArcItemSpec SpecB = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);
		FArcItemSpec SpecC = ArcCraftContextTestHelpers::MakeTestItemSpec(Def);

		TArray<FArcItemSpec> Ingredients = { SpecA, SpecB, SpecC };
		TArray<float> SlotRaritySteps = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, SlotRaritySteps, 1.0f);

		ASSERT_THAT(AreEqual(3, Ctx.PerSlotTags.Num(),
			TEXT("PerSlotTags should be sized to match ingredient count")));
	}
};
