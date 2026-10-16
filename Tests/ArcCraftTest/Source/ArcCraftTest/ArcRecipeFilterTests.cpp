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

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Station/ArcRecipeLookup.h"

#include "ArcCraftTestRecordFiles.h"

// ---------------------------------------------------------------------------
// Unique tag prefix: TAG_ArcRecipeFilter_*
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcRecipeFilter_Station_Hands, "Test.ArcRecipeFilter.Station.Hands");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcRecipeFilter_Station_Forge, "Test.ArcRecipeFilter.Station.Forge");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcRecipeFilter_Tier,          "Test.ArcRecipeFilter.Tier");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcRecipeFilter_Tier_0,        "Test.ArcRecipeFilter.Tier.0");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcRecipeFilter_Tier_2,        "Test.ArcRecipeFilter.Tier.2");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ArcRecipeFilter_Category_Tool, "Test.ArcRecipeFilter.Category.Tool");

// ---------------------------------------------------------------------------
// File-scoped namespace — unique across the project
// ---------------------------------------------------------------------------

namespace ArcRecipeFilterTestHelpers
{
	/**
	 * Write an indexed recipe record carrying the given RecipeTags and RequiredStationTags.
	 * FArcRecipeLookup::FindRecipesForStation reads them back from the record index, so
	 * the filter runs against the same records production resolves.
	 */
	FPrimaryAssetId WriteRecipe(
		FArcCraftTestRecordFiles& Records,
		const FGameplayTagContainer& InRequiredStationTags,
		const FGameplayTagContainer& InRecipeTags)
	{
		FArcRecipeDefinitionData Recipe;
		Recipe.RequiredStationTags = InRequiredStationTags;
		Recipe.RecipeTags          = InRecipeTags;
		return Records.Write(Recipe);
	}
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcRecipeFilter, "ArcCraft.RecipeFilter")
{
	FArcCraftTestRecordFiles Records;

	// =========================================================================
	// Case 1 — type-pass + tier-pass => recipe INCLUDED
	//   RequiredStationTags = {Station.Hands}, RecipeTags = {Tier.0}
	//   StationTypeTags     = {Station.Hands}, OfferedRecipeTags = {Tier.0}
	// =========================================================================
	TEST_METHOD(TypePass_TierPass_RecipeIncluded)
	{
		FGameplayTagContainer RequiredStation;
		RequiredStation.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const FPrimaryAssetId RecipeId = ArcRecipeFilterTestHelpers::WriteRecipe(Records, RequiredStation, RecipeTags);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));
		ASSERT_THAT(IsNotNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcRecipeDefinitionData>(RecipeId),
			TEXT("Indexed recipe id must resolve to its record")));

		FGameplayTagContainer StationType;
		StationType.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationType, Offered);

		ASSERT_THAT(IsTrue(Result.Contains(RecipeId),
			TEXT("Case1: recipe with matching station+tier must be INCLUDED")));
	}

	// =========================================================================
	// Case 2 — type-FAIL => recipe EXCLUDED
	//   RequiredStationTags = {Station.Forge}, RecipeTags = {Tier.0}
	//   StationTypeTags     = {Station.Hands}, OfferedRecipeTags = {Tier.0}
	// =========================================================================
	TEST_METHOD(TypeFail_RecipeExcluded)
	{
		FGameplayTagContainer RequiredStation;
		RequiredStation.AddTag(TAG_ArcRecipeFilter_Station_Forge);

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const FPrimaryAssetId RecipeId = ArcRecipeFilterTestHelpers::WriteRecipe(Records, RequiredStation, RecipeTags);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));
		ASSERT_THAT(IsNotNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcRecipeDefinitionData>(RecipeId),
			TEXT("Indexed recipe id must resolve to its record")));

		FGameplayTagContainer StationType;
		StationType.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationType, Offered);

		ASSERT_THAT(IsFalse(Result.Contains(RecipeId),
			TEXT("Case2: recipe requiring Forge must be EXCLUDED from a Hands station")));
	}

	// =========================================================================
	// Case 3 — RecipeTags EMPTY => recipe EXCLUDED
	//   RequiredStationTags = {Station.Hands}, RecipeTags = {}
	//   StationTypeTags     = {Station.Hands}, OfferedRecipeTags = {Tier.0}
	// =========================================================================
	TEST_METHOD(EmptyRecipeTags_RecipeExcluded)
	{
		FGameplayTagContainer RequiredStation;
		RequiredStation.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer RecipeTags; // empty

		const FPrimaryAssetId RecipeId = ArcRecipeFilterTestHelpers::WriteRecipe(Records, RequiredStation, RecipeTags);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));
		ASSERT_THAT(IsNotNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcRecipeDefinitionData>(RecipeId),
			TEXT("Indexed recipe id must resolve to its record")));

		FGameplayTagContainer StationType;
		StationType.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationType, Offered);

		ASSERT_THAT(IsFalse(Result.Contains(RecipeId),
			TEXT("Case3: recipe with empty RecipeTags must always be EXCLUDED")));
	}

	// =========================================================================
	// Case 4 — tier no-overlap => recipe EXCLUDED
	//   RequiredStationTags = {Station.Hands}, RecipeTags = {Category.Tool}
	//   StationTypeTags     = {Station.Hands}, OfferedRecipeTags = {Tier.0}
	// =========================================================================
	TEST_METHOD(TierNoOverlap_RecipeExcluded)
	{
		FGameplayTagContainer RequiredStation;
		RequiredStation.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_ArcRecipeFilter_Category_Tool);

		const FPrimaryAssetId RecipeId = ArcRecipeFilterTestHelpers::WriteRecipe(Records, RequiredStation, RecipeTags);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));
		ASSERT_THAT(IsNotNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcRecipeDefinitionData>(RecipeId),
			TEXT("Indexed recipe id must resolve to its record")));

		FGameplayTagContainer StationType;
		StationType.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationType, Offered);

		ASSERT_THAT(IsFalse(Result.Contains(RecipeId),
			TEXT("Case4: Category.Tool recipe must be EXCLUDED when only Tier.0 is offered")));
	}

	// =========================================================================
	// Case 5 — RequiredStationTags EMPTY + tier-pass => recipe INCLUDED
	//   RequiredStationTags = {}, RecipeTags = {Tier.0}
	//   StationTypeTags     = {Station.Hands}, OfferedRecipeTags = {Tier.0}
	// =========================================================================
	TEST_METHOD(EmptyRequiredStationTags_TierPass_RecipeIncluded)
	{
		FGameplayTagContainer RequiredStation; // empty — matches any station

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const FPrimaryAssetId RecipeId = ArcRecipeFilterTestHelpers::WriteRecipe(Records, RequiredStation, RecipeTags);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));
		ASSERT_THAT(IsNotNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcRecipeDefinitionData>(RecipeId),
			TEXT("Indexed recipe id must resolve to its record")));

		FGameplayTagContainer StationType;
		StationType.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationType, Offered);

		ASSERT_THAT(IsTrue(Result.Contains(RecipeId),
			TEXT("Case5: recipe with empty RequiredStationTags must be INCLUDED at any station when tier matches")));
	}

	// =========================================================================
	// Case 6 — parent-offer hierarchy => recipe INCLUDED
	//   RequiredStationTags = {Station.Hands}, RecipeTags = {Tier.0}
	//   StationTypeTags     = {Station.Hands}, OfferedRecipeTags = {Tier}
	//
	//   HasAny expands the receiver's tags (AssetRecipeTags) so child tag Tier.0
	//   is a child of offered parent tag Tier — the recipe must be INCLUDED.
	// =========================================================================
	TEST_METHOD(ParentOfferHierarchy_RecipeIncluded)
	{
		FGameplayTagContainer RequiredStation;
		RequiredStation.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer RecipeTags;
		RecipeTags.AddTag(TAG_ArcRecipeFilter_Tier_0); // child of Tier

		const FPrimaryAssetId RecipeId = ArcRecipeFilterTestHelpers::WriteRecipe(Records, RequiredStation, RecipeTags);
		ASSERT_THAT(IsTrue(RecipeId.IsValid(), TEXT("Recipe record must be written and indexed")));
		ASSERT_THAT(IsNotNull(UArcCoreAssetManager::Get().FindJsonRecord<FArcRecipeDefinitionData>(RecipeId),
			TEXT("Indexed recipe id must resolve to its record")));

		FGameplayTagContainer StationType;
		StationType.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_ArcRecipeFilter_Tier); // parent tag — should match child recipe tag

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationType, Offered);

		ASSERT_THAT(IsTrue(Result.Contains(RecipeId),
			TEXT("Case6: recipe tagged Tier.0 must be INCLUDED when station offers parent tag Tier")));
	}

	// =========================================================================
	// Bonus: multi-recipe array, only matching entries survive
	// =========================================================================
	TEST_METHOD(MultiRecipeArray_OnlyMatchingSurvive)
	{
		FGameplayTagContainer ReqHands;
		ReqHands.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer ReqForge;
		ReqForge.AddTag(TAG_ArcRecipeFilter_Station_Forge);

		FGameplayTagContainer TagTier0;
		TagTier0.AddTag(TAG_ArcRecipeFilter_Tier_0);

		FGameplayTagContainer TagTier2;
		TagTier2.AddTag(TAG_ArcRecipeFilter_Tier_2);

		// Should pass: Hands station, Tier.0 recipe
		const FPrimaryAssetId PassId = ArcRecipeFilterTestHelpers::WriteRecipe(Records, ReqHands, TagTier0);
		// Should fail type gate: requires Forge
		const FPrimaryAssetId ForgeId = ArcRecipeFilterTestHelpers::WriteRecipe(Records, ReqForge, TagTier0);
		// Should fail tier gate: Tier.2 not in offered {Tier.0}
		const FPrimaryAssetId Tier2Id = ArcRecipeFilterTestHelpers::WriteRecipe(Records, ReqHands, TagTier2);
		ASSERT_THAT(IsTrue(PassId.IsValid() && ForgeId.IsValid() && Tier2Id.IsValid(),
			TEXT("Recipe records must be written and indexed")));

		FGameplayTagContainer StationType;
		StationType.AddTag(TAG_ArcRecipeFilter_Station_Hands);

		FGameplayTagContainer Offered;
		Offered.AddTag(TAG_ArcRecipeFilter_Tier_0);

		const TArray<FPrimaryAssetId> Result = FArcRecipeLookup::FindRecipesForStation(StationType, Offered);

		ASSERT_THAT(IsTrue(Result.Contains(PassId),
			TEXT("MultiRecipe: the Hands+Tier.0 recipe must survive")));
		ASSERT_THAT(IsFalse(Result.Contains(ForgeId),
			TEXT("MultiRecipe: the Forge recipe must fail the type gate")));
		ASSERT_THAT(IsFalse(Result.Contains(Tier2Id),
			TEXT("MultiRecipe: the Tier.2 recipe must fail the tier gate")));
	}
};
