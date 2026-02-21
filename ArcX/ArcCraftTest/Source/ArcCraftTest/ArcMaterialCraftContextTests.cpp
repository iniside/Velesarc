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
#include "Items/ArcItemData.h"

// Tags for context tests — unique names to avoid ODR with evaluator test tags
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Metal, "Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Gem, "Resource.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Wood, "Resource.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Recipe_Sword, "Recipe.Weapon.Sword");

// ===================================================================
// FArcMaterialCraftContext::Build tests
// ===================================================================

TEST_CLASS(MaterialCraft_ContextBuild, "ArcCraft.MaterialCraft.Context")
{
	TEST_METHOD(Build_PopulatesPerSlotTags)
	{
		// Build() reads tags via ArcItemsHelper::GetFragment<FArcItemFragment_Tags>()
		// which requires item definitions. Test items without definitions won't have
		// tags populated by Build(), so we set PerSlotTags directly.
		FArcItemData ItemA;
		FArcItemData ItemB;

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB };
		TArray<float> QualityMults = { 1.0f, 1.2f };

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_CtxTest_Recipe_Sword);

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.1f, RecipeTags, 2, 0.5f);

		// Simulate what Build() would do with real item definitions
		Ctx.PerSlotTags[0].AddTag(TAG_CtxTest_Resource_Metal);
		Ctx.PerSlotTags[1].AddTag(TAG_CtxTest_Resource_Gem);

		ASSERT_THAT(IsTrue(Ctx.PerSlotTags[0].HasTag(TAG_CtxTest_Resource_Metal),
			TEXT("Slot 0 should have Metal tag from item A")));
		ASSERT_THAT(IsTrue(Ctx.PerSlotTags[1].HasTag(TAG_CtxTest_Resource_Gem),
			TEXT("Slot 1 should have Gem tag from item B")));
	}

	TEST_METHOD(Build_CopiesRecipeTags)
	{
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_CtxTest_Recipe_Sword);

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, RecipeTags);

		ASSERT_THAT(IsTrue(Ctx.RecipeTags.HasTag(TAG_CtxTest_Recipe_Sword),
			TEXT("Should copy recipe tags")));
	}

	TEST_METHOD(Build_SetsQualityMultipliers)
	{
		FArcItemData ItemA;

		TArray<const FArcItemData*> Ingredients = { &ItemA };
		TArray<float> QualityMults = { 1.5f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.5f, FGameplayTagContainer());

		ASSERT_THAT(AreEqual(1, Ctx.IngredientQualityMultipliers.Num()));
		ASSERT_THAT(IsNear(1.5f, Ctx.IngredientQualityMultipliers[0], 0.001f));
	}

	TEST_METHOD(Build_SetsIngredientCount)
	{
		FArcItemData ItemA;
		FArcItemData ItemB;
		FArcItemData ItemC;

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB, &ItemC };
		TArray<float> QualityMults = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer());

		ASSERT_THAT(AreEqual(3, Ctx.IngredientCount, TEXT("Should track ingredient count")));
	}

	TEST_METHOD(Build_BaseIngredientCount_ZeroUsesActual)
	{
		FArcItemData ItemA;
		FArcItemData ItemB;

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB };
		TArray<float> QualityMults = { 1.0f, 1.0f };

		// BaseIngredientCount = 0 means use actual count
		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer(), /*BaseIngredientCount=*/ 0);

		ASSERT_THAT(AreEqual(2, Ctx.BaseIngredientCount,
			TEXT("BaseIngredientCount=0 should use actual ingredient count")));
	}

	TEST_METHOD(Build_BaseIngredientCount_ExplicitValue)
	{
		FArcItemData ItemA;
		FArcItemData ItemB;
		FArcItemData ItemC;

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB, &ItemC };
		TArray<float> QualityMults = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer(), /*BaseIngredientCount=*/ 2);

		ASSERT_THAT(AreEqual(2, Ctx.BaseIngredientCount,
			TEXT("BaseIngredientCount should be set to explicit value")));
		ASSERT_THAT(AreEqual(3, Ctx.IngredientCount,
			TEXT("IngredientCount should be actual count")));
	}

	TEST_METHOD(Build_SetsExtraCraftTimeBonus)
	{
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer(), 0, /*ExtraCraftTimeBonus=*/ 0.75f);

		ASSERT_THAT(IsNear(0.75f, Ctx.ExtraCraftTimeBonus, 0.001f));
	}

	TEST_METHOD(Build_SetsAverageQuality)
	{
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 2.3f, FGameplayTagContainer());

		ASSERT_THAT(IsNear(2.3f, Ctx.AverageQuality, 0.001f));
		ASSERT_THAT(IsNear(2.3f, Ctx.BandEligibilityQuality, 0.001f,
			TEXT("BandEligibilityQuality should initially equal AverageQuality")));
	}

	TEST_METHOD(Build_SetsBandWeightBonusToZero)
	{
		TArray<const FArcItemData*> Ingredients;
		TArray<float> QualityMults;

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.5f, FGameplayTagContainer());

		ASSERT_THAT(IsNear(0.0f, Ctx.BandWeightBonus, 0.001f,
			TEXT("BandWeightBonus should be zero after Build (computed by evaluator)")));
	}

	TEST_METHOD(Build_NullIngredient_SkippedSafely)
	{
		FArcItemData ItemA;

		TArray<const FArcItemData*> Ingredients = { &ItemA, nullptr };
		TArray<float> QualityMults = { 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer());

		// Simulate tags for non-null slot (Build reads from item definition which test items lack)
		Ctx.PerSlotTags[0].AddTag(TAG_CtxTest_Resource_Wood);

		ASSERT_THAT(IsTrue(Ctx.PerSlotTags[0].HasTag(TAG_CtxTest_Resource_Wood),
			TEXT("Should have tags in slot for non-null ingredient")));
		ASSERT_THAT(AreEqual(0, Ctx.PerSlotTags[1].Num(),
			TEXT("Null ingredient slot should have empty tags")));
		ASSERT_THAT(AreEqual(2, Ctx.IngredientCount,
			TEXT("IngredientCount includes null entries in the array")));
		ASSERT_THAT(AreEqual(2, Ctx.PerSlotTags.Num(),
			TEXT("PerSlotTags array should match ingredient count")));
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
		// Each slot independently has the tag — they are not deduplicated across slots
		ASSERT_THAT(AreEqual(1, Ctx.PerSlotTags[0].Num(),
			TEXT("Slot 0 should have exactly one tag")));
		ASSERT_THAT(AreEqual(1, Ctx.PerSlotTags[1].Num(),
			TEXT("Slot 1 should have exactly one tag")));
	}

	TEST_METHOD(Build_AllocatesPerSlotTagsArray)
	{
		FArcItemData ItemA;
		FArcItemData ItemB;
		FArcItemData ItemC;

		TArray<const FArcItemData*> Ingredients = { &ItemA, &ItemB, &ItemC };
		TArray<float> QualityMults = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer());

		ASSERT_THAT(AreEqual(3, Ctx.PerSlotTags.Num(),
			TEXT("PerSlotTags should be sized to match ingredient count")));
	}
};
