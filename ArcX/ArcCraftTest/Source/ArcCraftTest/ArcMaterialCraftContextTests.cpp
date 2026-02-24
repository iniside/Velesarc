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
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

// Tags for context tests -- unique names to avoid ODR with evaluator test tags
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Metal, "Resource.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Gem, "Resource.Gem");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Resource_Wood, "Resource.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CtxTest_Recipe_Sword, "Recipe.Weapon.Sword");

// ===================================================================
// Helper: create transient UArcItemDefinition with optional item tags
// ===================================================================

namespace ArcCraftContextTestHelpers
{
	/**
	 * Create a transient UArcItemDefinition and optionally inject an
	 * FArcItemFragment_Tags with the given ItemTags.
	 */
	UArcItemDefinition* CreateTransientItemDef(
		const FName& Name,
		const FGameplayTagContainer& ItemTags = FGameplayTagContainer())
	{
		UArcItemDefinition* Def = NewObject<UArcItemDefinition>(
			GetTransientPackage(), Name, RF_Transient);
		Def->RegenerateItemId();

		if (ItemTags.Num() > 0)
		{
			FArcItemFragment_Tags TagsFragment;
			TagsFragment.ItemTags = ItemTags;
			Def->AddFragment(TagsFragment);
		}

		return Def;
	}

	/**
	 * Create an FArcItemData backed by a transient item definition.
	 * Uses FArcItemData::NewFromSpec to properly initialize the item.
	 */
	TSharedPtr<FArcItemData> MakeTestItem(
		const UArcItemDefinition* ItemDef)
	{
		FArcItemSpec Spec;
		Spec.SetItemDefinitionAsset(ItemDef);
		return FArcItemData::NewFromSpec(Spec);
	}
}

// ===================================================================
// FArcMaterialCraftContext::Build tests
// ===================================================================

TEST_CLASS(MaterialCraft_ContextBuild, "ArcCraft.MaterialCraft.Context")
{
	TEST_METHOD(Build_PopulatesPerSlotTags)
	{
		// Create item definitions with actual tags so Build() reads them
		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_CtxTest_Resource_Metal);
		UArcItemDefinition* MetalDef = ArcCraftContextTestHelpers::CreateTransientItemDef(
			TEXT("TestItem_Metal"), MetalTags);

		FGameplayTagContainer GemTags;
		GemTags.AddTag(TAG_CtxTest_Resource_Gem);
		UArcItemDefinition* GemDef = ArcCraftContextTestHelpers::CreateTransientItemDef(
			TEXT("TestItem_Gem"), GemTags);

		TSharedPtr<FArcItemData> ItemA = ArcCraftContextTestHelpers::MakeTestItem(MetalDef);
		TSharedPtr<FArcItemData> ItemB = ArcCraftContextTestHelpers::MakeTestItem(GemDef);

		TArray<const FArcItemData*> Ingredients = { ItemA.Get(), ItemB.Get() };
		TArray<float> QualityMults = { 1.0f, 1.2f };

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_CtxTest_Recipe_Sword);

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.1f, RecipeTags, 2, 0.5f);

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
		UArcItemDefinition* Def = ArcCraftContextTestHelpers::CreateTransientItemDef(TEXT("TestItem_Qual"));
		TSharedPtr<FArcItemData> ItemA = ArcCraftContextTestHelpers::MakeTestItem(Def);

		TArray<const FArcItemData*> Ingredients = { ItemA.Get() };
		TArray<float> QualityMults = { 1.5f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.5f, FGameplayTagContainer());

		ASSERT_THAT(AreEqual(1, Ctx.IngredientQualityMultipliers.Num()));
		ASSERT_THAT(IsNear(1.5f, Ctx.IngredientQualityMultipliers[0], 0.001f));
	}

	TEST_METHOD(Build_SetsIngredientCount)
	{
		UArcItemDefinition* Def = ArcCraftContextTestHelpers::CreateTransientItemDef(TEXT("TestItem_Count"));
		TSharedPtr<FArcItemData> ItemA = ArcCraftContextTestHelpers::MakeTestItem(Def);
		TSharedPtr<FArcItemData> ItemB = ArcCraftContextTestHelpers::MakeTestItem(Def);
		TSharedPtr<FArcItemData> ItemC = ArcCraftContextTestHelpers::MakeTestItem(Def);

		TArray<const FArcItemData*> Ingredients = { ItemA.Get(), ItemB.Get(), ItemC.Get() };
		TArray<float> QualityMults = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer());

		ASSERT_THAT(AreEqual(3, Ctx.IngredientCount, TEXT("Should track ingredient count")));
	}

	TEST_METHOD(Build_BaseIngredientCount_ZeroUsesActual)
	{
		UArcItemDefinition* Def = ArcCraftContextTestHelpers::CreateTransientItemDef(TEXT("TestItem_Base0"));
		TSharedPtr<FArcItemData> ItemA = ArcCraftContextTestHelpers::MakeTestItem(Def);
		TSharedPtr<FArcItemData> ItemB = ArcCraftContextTestHelpers::MakeTestItem(Def);

		TArray<const FArcItemData*> Ingredients = { ItemA.Get(), ItemB.Get() };
		TArray<float> QualityMults = { 1.0f, 1.0f };

		// BaseIngredientCount = 0 means use actual count
		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer(), /*BaseIngredientCount=*/ 0);

		ASSERT_THAT(AreEqual(2, Ctx.BaseIngredientCount,
			TEXT("BaseIngredientCount=0 should use actual ingredient count")));
	}

	TEST_METHOD(Build_BaseIngredientCount_ExplicitValue)
	{
		UArcItemDefinition* Def = ArcCraftContextTestHelpers::CreateTransientItemDef(TEXT("TestItem_BaseN"));
		TSharedPtr<FArcItemData> ItemA = ArcCraftContextTestHelpers::MakeTestItem(Def);
		TSharedPtr<FArcItemData> ItemB = ArcCraftContextTestHelpers::MakeTestItem(Def);
		TSharedPtr<FArcItemData> ItemC = ArcCraftContextTestHelpers::MakeTestItem(Def);

		TArray<const FArcItemData*> Ingredients = { ItemA.Get(), ItemB.Get(), ItemC.Get() };
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
		FGameplayTagContainer WoodTags;
		WoodTags.AddTag(TAG_CtxTest_Resource_Wood);
		UArcItemDefinition* WoodDef = ArcCraftContextTestHelpers::CreateTransientItemDef(
			TEXT("TestItem_Wood"), WoodTags);
		TSharedPtr<FArcItemData> ItemA = ArcCraftContextTestHelpers::MakeTestItem(WoodDef);

		TArray<const FArcItemData*> Ingredients = { ItemA.Get(), nullptr };
		TArray<float> QualityMults = { 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer());

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
		// Each slot independently has the tag -- they are not deduplicated across slots
		ASSERT_THAT(AreEqual(1, Ctx.PerSlotTags[0].Num(),
			TEXT("Slot 0 should have exactly one tag")));
		ASSERT_THAT(AreEqual(1, Ctx.PerSlotTags[1].Num(),
			TEXT("Slot 1 should have exactly one tag")));
	}

	TEST_METHOD(Build_AllocatesPerSlotTagsArray)
	{
		UArcItemDefinition* Def = ArcCraftContextTestHelpers::CreateTransientItemDef(TEXT("TestItem_Alloc"));
		TSharedPtr<FArcItemData> ItemA = ArcCraftContextTestHelpers::MakeTestItem(Def);
		TSharedPtr<FArcItemData> ItemB = ArcCraftContextTestHelpers::MakeTestItem(Def);
		TSharedPtr<FArcItemData> ItemC = ArcCraftContextTestHelpers::MakeTestItem(Def);

		TArray<const FArcItemData*> Ingredients = { ItemA.Get(), ItemB.Get(), ItemC.Get() };
		TArray<float> QualityMults = { 1.0f, 1.0f, 1.0f };

		FArcMaterialCraftContext Ctx = FArcMaterialCraftContext::Build(
			Ingredients, QualityMults, 1.0f, FGameplayTagContainer());

		ASSERT_THAT(AreEqual(3, Ctx.PerSlotTags.Num(),
			TEXT("PerSlotTags should be sized to match ingredient count")));
	}
};
