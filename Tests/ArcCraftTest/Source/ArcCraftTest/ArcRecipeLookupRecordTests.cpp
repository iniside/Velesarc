// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "ArcCraft/Station/ArcRecipeLookup.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "ArcCraftTestRecordFiles.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecLookup_Station, "Test.RecLookup.Station");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecLookup_OtherStation, "Test.RecLookup.OtherStation");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecLookup_Tier, "Test.RecLookup.Tier");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecLookup_Ore, "Test.RecLookup.Ore");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecLookup_Gem, "Test.RecLookup.Gem");

namespace ArcRecipeLookupRecordTestsImpl
{
	FPrimaryAssetType CustomRecipeType()
	{
		return FPrimaryAssetType(TEXT("ArcTestCraftRecipeType"));
	}

	void AddTagIngredient(FArcRecipeDefinitionData& Recipe, const FGameplayTag& Tag)
	{
		FArcRecipeIngredient_Tags Ingredient;
		Ingredient.Amount = 1;
		Ingredient.RequiredTags.AddTag(Tag);
		Recipe.Ingredients.Add(FInstancedStruct::Make(Ingredient));
	}

	FArcRecipeDefinitionData MakeStationRecipe(const FGameplayTag& RequiredStationTag, bool bCustomType)
	{
		FArcRecipeDefinitionData Recipe;
		Recipe.RequiredStationTags.AddTag(RequiredStationTag);
		Recipe.RecipeTags.AddTag(TAG_RecLookup_Tier);
		if (bCustomType)
		{
			Recipe.RecipeType = CustomRecipeType();
		}
		return Recipe;
	}

	FArcRecipeDefinitionData MakeIngredientRecipe(bool bCustomType, const FGameplayTag& FirstTag, const FGameplayTag& SecondTag)
	{
		FArcRecipeDefinitionData Recipe;
		if (bCustomType)
		{
			Recipe.RecipeType = CustomRecipeType();
		}
		if (FirstTag.IsValid())
		{
			AddTagIngredient(Recipe, FirstTag);
		}
		if (SecondTag.IsValid())
		{
			AddTagIngredient(Recipe, SecondTag);
		}
		return Recipe;
	}

	FArcItemSpec MakeTaggedItem(FArcItemTestRecordFiles& ItemRecords, const FGameplayTag& Tag)
	{
		FArcItemDefinitionData Def;

		FArcItemFragment_Tags TagsFragment;
		TagsFragment.AssetTags.AddTag(Tag);
		ArcItemTestRecords::AddFragment(Def, TagsFragment);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(ItemRecords.Write(Def));
		Spec.Amount = 1;
		return Spec;
	}
}

TEST_CLASS(ArcRecipeLookupRecords, "ArcCraft.RecipeLookup.Records")
{
	FArcItemTestRecordFiles ItemRecords;
	FArcCraftTestRecordFiles Records;

	TEST_METHOD(FindRecipesForStation_CustomRecipeType_IsFound)
	{
		const FPrimaryAssetId CustomId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeStationRecipe(TAG_RecLookup_Station, true));
		const FPrimaryAssetId DefaultId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeStationRecipe(TAG_RecLookup_Station, false));
		ASSERT_THAT(IsTrue(CustomId.IsValid() && DefaultId.IsValid(), TEXT("Recipe records must be written and indexed")));
		ASSERT_THAT(IsTrue(CustomId.PrimaryAssetType == ArcRecipeLookupRecordTestsImpl::CustomRecipeType(), TEXT("Custom recipe id must carry the custom RecipeType")));
		ASSERT_THAT(IsNotNull(ArcCraftRecords::Find<FArcRecipeDefinitionData>(CustomId), TEXT("Custom-type id must resolve to its record")));

		FGameplayTagContainer StationTags;
		StationTags.AddTag(TAG_RecLookup_Station);
		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_RecLookup_Tier);

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationTags, Offered);

		ASSERT_THAT(IsTrue(Result.Contains(CustomId), TEXT("Recipe with a custom RecipeType must be found for the station")));
		ASSERT_THAT(IsTrue(Result.Contains(DefaultId), TEXT("Recipe with the default RecipeType must still be found")));
	}

	TEST_METHOD(FindRecipesForStation_CustomRecipeType_OtherStationExcluded)
	{
		const FPrimaryAssetId CustomId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeStationRecipe(TAG_RecLookup_OtherStation, true));
		ASSERT_THAT(IsTrue(CustomId.IsValid(), TEXT("Recipe record must be written and indexed")));

		FGameplayTagContainer StationTags;
		StationTags.AddTag(TAG_RecLookup_Station);
		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_RecLookup_Tier);

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationTags, Offered);

		ASSERT_THAT(IsFalse(Result.Contains(CustomId), TEXT("Custom-type recipe requiring another station must be excluded")));
	}

	TEST_METHOD(FilterByAvailableIngredients_KeepsOnlyOverlappingAndIngredientFree)
	{
		const FPrimaryAssetId OreId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeIngredientRecipe(true, TAG_RecLookup_Ore.GetTag(), FGameplayTag()));
		const FPrimaryAssetId GemId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeIngredientRecipe(false, TAG_RecLookup_Gem.GetTag(), FGameplayTag()));
		const FPrimaryAssetId FreeId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeIngredientRecipe(true, FGameplayTag(), FGameplayTag()));
		ASSERT_THAT(IsTrue(OreId.IsValid() && GemId.IsValid() && FreeId.IsValid(), TEXT("Recipe records must be written and indexed")));

		FGameplayTagContainer Available;
		Available.AddTag(TAG_RecLookup_Ore);

		const TArray<FPrimaryAssetId> Candidates = { OreId, GemId, FreeId };
		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FilterByAvailableIngredients(Candidates, Available);

		ASSERT_THAT(IsTrue(Result.Contains(OreId), TEXT("Custom-type recipe whose ingredient tag is available must be kept")));
		ASSERT_THAT(IsFalse(Result.Contains(GemId), TEXT("Recipe whose ingredient tag is not available must be dropped")));
		ASSERT_THAT(IsTrue(Result.Contains(FreeId), TEXT("Recipe without tag ingredients must be kept")));
	}

	TEST_METHOD(FindBestRecipeForIngredients_AllSlotsSatisfiable_PicksRicherRecipe)
	{
		const FPrimaryAssetId OreOnlyId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeIngredientRecipe(true, TAG_RecLookup_Ore.GetTag(), FGameplayTag()));
		const FPrimaryAssetId OreGemId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeIngredientRecipe(false, TAG_RecLookup_Ore.GetTag(), TAG_RecLookup_Gem.GetTag()));
		ASSERT_THAT(IsTrue(OreOnlyId.IsValid() && OreGemId.IsValid(), TEXT("Recipe records must be written and indexed")));

		const TArray<FArcItemSpec> Items = {
			ArcRecipeLookupRecordTestsImpl::MakeTaggedItem(ItemRecords, TAG_RecLookup_Ore.GetTag()),
			ArcRecipeLookupRecordTestsImpl::MakeTaggedItem(ItemRecords, TAG_RecLookup_Gem.GetTag())
		};
		for (const FArcItemSpec& Item : Items)
		{
			ASSERT_THAT(IsTrue(Item.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		}
		const TArray<FPrimaryAssetId> Candidates = { OreOnlyId, OreGemId };

		float Score = 0.0f;
		const FPrimaryAssetId Best = FArcRecipeLookup::FindBestRecipeForIngredients(Candidates, Items, FGameplayTagContainer(), Score);

		ASSERT_THAT(IsTrue(Best == OreGemId, TEXT("The two-slot recipe satisfied by both items must win")));
		ASSERT_THAT(IsTrue(Score > 0.0f, TEXT("A match must report a positive score")));
	}

	TEST_METHOD(FindBestRecipeForIngredients_RicherRecipeUnsatisfied_PicksCustomTypeRecipe)
	{
		const FPrimaryAssetId OreOnlyId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeIngredientRecipe(true, TAG_RecLookup_Ore.GetTag(), FGameplayTag()));
		const FPrimaryAssetId OreGemId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeIngredientRecipe(false, TAG_RecLookup_Ore.GetTag(), TAG_RecLookup_Gem.GetTag()));
		ASSERT_THAT(IsTrue(OreOnlyId.IsValid() && OreGemId.IsValid(), TEXT("Recipe records must be written and indexed")));

		const TArray<FArcItemSpec> Items = {
			ArcRecipeLookupRecordTestsImpl::MakeTaggedItem(ItemRecords, TAG_RecLookup_Ore.GetTag())
		};
		for (const FArcItemSpec& Item : Items)
		{
			ASSERT_THAT(IsTrue(Item.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		}
		const TArray<FPrimaryAssetId> Candidates = { OreGemId, OreOnlyId };

		float Score = 0.0f;
		const FPrimaryAssetId Best = FArcRecipeLookup::FindBestRecipeForIngredients(Candidates, Items, FGameplayTagContainer(), Score);

		ASSERT_THAT(IsTrue(Best == OreOnlyId, TEXT("Only the custom-type one-slot recipe is satisfiable and must be returned with its full id")));
		ASSERT_THAT(IsTrue(Score > 0.0f, TEXT("A match must report a positive score")));
	}

	TEST_METHOD(FindBestRecipeForIngredients_UnresolvedCandidate_IsSkipped)
	{
		const FPrimaryAssetId OreOnlyId = Records.Write(ArcRecipeLookupRecordTestsImpl::MakeIngredientRecipe(false, TAG_RecLookup_Ore.GetTag(), FGameplayTag()));
		ASSERT_THAT(IsTrue(OreOnlyId.IsValid(), TEXT("Recipe record must be written and indexed")));

		const FPrimaryAssetId MissingId(FPrimaryAssetType(TEXT("ArcRecipe")), FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
		const TArray<FArcItemSpec> Items = {
			ArcRecipeLookupRecordTestsImpl::MakeTaggedItem(ItemRecords, TAG_RecLookup_Ore.GetTag())
		};
		for (const FArcItemSpec& Item : Items)
		{
			ASSERT_THAT(IsTrue(Item.GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		}

		float Score = 0.0f;
		const FPrimaryAssetId BestWithMissing = FArcRecipeLookup::FindBestRecipeForIngredients(
			TArray<FPrimaryAssetId>({ MissingId, OreOnlyId }), Items, FGameplayTagContainer(), Score);
		ASSERT_THAT(IsTrue(BestWithMissing == OreOnlyId, TEXT("An unresolved candidate must be skipped without hiding the resolved one")));

		const FPrimaryAssetId BestOfMissingOnly = FArcRecipeLookup::FindBestRecipeForIngredients(
			TArray<FPrimaryAssetId>({ MissingId }), Items, FGameplayTagContainer(), Score);
		ASSERT_THAT(IsFalse(BestOfMissingOnly.IsValid(), TEXT("Only unresolved candidates must yield no match")));
		ASSERT_THAT(IsNear(0.0f, Score, 0.0001f, TEXT("No match must report a zero score")));
	}
};
