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

// ArcRecipeGraphBuilderTests.cpp
// CQTest suite for FArcRecipeGraphBuilder::BuildFromRecords and BuildFocused.
// No AssetRegistry, no UEdGraph, no Slate.

#if WITH_EDITOR

#include "CQTest.h"

#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcItemTestRecordFiles.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

#include "RecipeGraph/ArcRecipeGraphBuilder.h"
#include "RecipeGraph/ArcRecipeGraphData.h"

#include "StructUtils/InstancedStruct.h"

// ---------------------------------------------------------------------------
// Unique gameplay tags for this test file — prefix TAG_GraphTest_*
// ---------------------------------------------------------------------------

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GraphTest_Metal,       "Test.GraphTest.Metal");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GraphTest_Metal_Iron,  "Test.GraphTest.Metal.Iron");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GraphTest_Metal_Gold,  "Test.GraphTest.Metal.Gold");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GraphTest_Wood,        "Test.GraphTest.Wood");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_GraphTest_Deny_Rare,   "Test.GraphTest.Deny.Rare");

// ---------------------------------------------------------------------------
// File-scoped helpers (unique namespace)
// ---------------------------------------------------------------------------

namespace ArcRecipeGraphBuilderTestHelpers
{
	/** An in-memory item record together with the id the graph addresses it by. */
	struct FGraphTestItem
	{
		FPrimaryAssetId Id;
		FText Title;
		FArcItemDefinitionData Data;
	};

	/**
	 * Create an in-memory item record with the given primary asset type and a unique
	 * ItemId, and optionally add an FArcItemFragment_Tags with the supplied AssetTags.
	 */
	static FGraphTestItem MakeItem(
		const FName& ObjectName,
		FPrimaryAssetType ItemType,
		const FGameplayTagContainer& AssetTags)
	{
		FGraphTestItem Item;
		Item.Data.ItemType = ItemType;
		Item.Data.ItemId = FGuid::NewGuid();
		Item.Id = Item.Data.GetPrimaryAssetId();
		Item.Title = FText::FromName(ObjectName);

		if (!AssetTags.IsEmpty())
		{
			FArcItemFragment_Tags TagsFrag;
			TagsFrag.AssetTags = AssetTags;
			ArcItemTestRecords::AddFragment(Item.Data, TagsFrag);
		}

		return Item;
	}

	/** Entry the graph builder consumes; points into Item, which must outlive the build. */
	static FArcRecipeGraphItem MakeGraphItem(const FGraphTestItem& Item)
	{
		return FArcRecipeGraphItem{ Item.Id, Item.Title, &Item.Data };
	}

	/** An in-memory recipe record together with the id the graph addresses it by. */
	struct FGraphTestRecipe
	{
		FPrimaryAssetId Id;
		FArcRecipeDefinitionData Data;
	};

	/**
	 * Create an in-memory recipe record.
	 * The main output is the supplied item's PrimaryAssetId.
	 * Ingredients is left empty — callers add them.
	 */
	static FGraphTestRecipe MakeRecipe(
		const FName& ObjectName,
		FPrimaryAssetType RecipeType,
		const FGraphTestItem& OutputItem)
	{
		FGraphTestRecipe Recipe;
		Recipe.Data.RecipeType = RecipeType;

		// Unique RecipeId so the id (= (RecipeType, RecipeId digits)) does not collide across fixtures.
		Recipe.Data.RecipeId = FGuid::NewGuid();
		Recipe.Id = FPrimaryAssetId(RecipeType, FName(*Recipe.Data.RecipeId.ToString(EGuidFormats::Digits)));

		Recipe.Data.RecipeName = FText::FromName(ObjectName);

		FArcRecipeOutput& MainOutput = Recipe.Data.Outputs.AddDefaulted_GetRef();
		MainOutput.ItemDefinition.AssetId = OutputItem.Id;

		return Recipe;
	}

	/** Entry the graph builder consumes; points into Recipe, which must outlive the build. */
	static FArcRecipeGraphRecipe MakeGraphRecipe(const FGraphTestRecipe& Recipe)
	{
		FArcRecipeGraphRecipe Entry;
		Entry.Id = Recipe.Id;
		Entry.Recipe = &Recipe.Data;
		return Entry;
	}

	/** Add an _ItemDef ingredient to an existing recipe. */
	static void AddItemDefIngredient(
		FGraphTestRecipe& Recipe,
		const FGraphTestItem& Item,
		int32 Amount,
		bool bConsume)
	{
		FArcRecipeIngredient_ItemDef Ing;
		Ing.Amount         = Amount;
		Ing.bConsumeOnCraft = bConsume;
		Ing.ItemDefinitionId.AssetId = Item.Id;

		FInstancedStruct IngStruct = FInstancedStruct::Make<FArcRecipeIngredient_ItemDef>(Ing);
		Recipe.Data.Ingredients.Add(IngStruct);
	}

	/** Add a _Tags ingredient to an existing recipe. */
	static void AddTagIngredient(
		FGraphTestRecipe& Recipe,
		const FGameplayTagContainer& Required,
		const FGameplayTagContainer& Deny,
		int32 Amount,
		float MinRarityStep)
	{
		FArcRecipeIngredient_Tags Ing;
		Ing.Amount         = Amount;
		Ing.bConsumeOnCraft = true;
		Ing.RequiredTags   = Required;
		Ing.DenyTags       = Deny;
		Ing.MinRarityStep     = MinRarityStep;

		FInstancedStruct IngStruct = FInstancedStruct::Make<FArcRecipeIngredient_Tags>(Ing);
		Recipe.Data.Ingredients.Add(IngStruct);
	}

	static const FPrimaryAssetType ItemAssetType(TEXT("ArcItem"));
	static const FPrimaryAssetType RecipeAssetType(TEXT("ArcRecipe"));

} // namespace ArcRecipeGraphBuilderTestHelpers

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcRecipeGraphBuilderTest, "ArcCraft.RecipeGraph")
{
	// =========================================================================
	// Case (a): crafted item input — no RawItem node, direct recipe->recipe edge
	//
	//   ItemA produced by RecipeA
	//   RecipeB consumes ItemA via _ItemDef ingredient
	//   => expect Recipe->Recipe edge, no RawItem node for ItemA
	// =========================================================================
	TEST_METHOD(CraftedItemDef_DirectRecipeToRecipeEdge)
	{
		using namespace ArcRecipeGraphBuilderTestHelpers;

		const FGraphTestItem ItemA = MakeItem(TEXT("GT_ItemA_Crafted"), ItemAssetType, FGameplayTagContainer{});
		const FGraphTestItem ItemB = MakeItem(TEXT("GT_ItemB_Crafted_Output"), ItemAssetType, FGameplayTagContainer{});

		FGraphTestRecipe RecipeA = MakeRecipe(TEXT("GT_RecipeA_Producer"), RecipeAssetType, ItemA);
		FGraphTestRecipe RecipeB = MakeRecipe(TEXT("GT_RecipeB_Consumer"), RecipeAssetType, ItemB);
		AddItemDefIngredient(RecipeB, ItemA, 2, true);

		TArray<FArcRecipeGraphRecipe> Recipes = { MakeGraphRecipe(RecipeA), MakeGraphRecipe(RecipeB) };
		TArray<FArcRecipeGraphItem> Items = { MakeGraphItem(ItemA), MakeGraphItem(ItemB) };

		FArcRecipeGraphData Data = FArcRecipeGraphBuilder::BuildFromRecords(Recipes, Items);

		// Must have exactly 2 Recipe nodes — no RawItem node for ItemA
		int32 RecipeNodeCount = 0;
		int32 RawItemCount    = 0;
		for (const FArcRecipeGraphNodeData& Node : Data.Nodes)
		{
			if (Node.Type == EArcRecipeGraphNodeType::Recipe)   { ++RecipeNodeCount; }
			if (Node.Type == EArcRecipeGraphNodeType::RawItem)  { ++RawItemCount;    }
		}
		ASSERT_THAT(AreEqual(2, RecipeNodeCount, TEXT("(a) must have 2 Recipe nodes")));
		ASSERT_THAT(AreEqual(0, RawItemCount,    TEXT("(a) must have 0 RawItem nodes")));

		// Must have exactly 1 edge: RecipeA -> RecipeB
		bool bEdgeFound = false;
		for (const FArcRecipeGraphEdgeData& Edge : Data.Edges)
		{
			if (Edge.FromNode == RecipeA.Id &&
				Edge.ToNode   == RecipeB.Id)
			{
				bEdgeFound = true;
				ASSERT_THAT(AreEqual(2, Edge.Amount, TEXT("(a) edge amount must be 2")));
				break;
			}
		}
		ASSERT_THAT(IsTrue(bEdgeFound, TEXT("(a) recipe->recipe edge not found")));
	}

	// =========================================================================
	// Case (b): raw _ItemDef input — RawItem node + edge, no recipe->recipe edge
	//
	//   ItemRaw is not produced by any recipe
	//   RecipeC consumes ItemRaw via _ItemDef
	//   => RawItem node for ItemRaw + edge ItemRaw->RecipeC
	// =========================================================================
	TEST_METHOD(RawItemDef_RawItemNodeAndEdge)
	{
		using namespace ArcRecipeGraphBuilderTestHelpers;

		const FGraphTestItem ItemRaw    = MakeItem(TEXT("GT_ItemRaw"), ItemAssetType, FGameplayTagContainer{});
		const FGraphTestItem ItemOutput = MakeItem(TEXT("GT_ItemC_Output"), ItemAssetType, FGameplayTagContainer{});

		FGraphTestRecipe RecipeC = MakeRecipe(TEXT("GT_RecipeC_RawConsumer"), RecipeAssetType, ItemOutput);
		AddItemDefIngredient(RecipeC, ItemRaw, 1, true);

		TArray<FArcRecipeGraphRecipe> Recipes = { MakeGraphRecipe(RecipeC) };
		TArray<FArcRecipeGraphItem> Items = { MakeGraphItem(ItemRaw), MakeGraphItem(ItemOutput) };

		FArcRecipeGraphData Data = FArcRecipeGraphBuilder::BuildFromRecords(Recipes, Items);

		// Must have 1 Recipe node and 1 RawItem node
		int32 RecipeCount  = 0;
		int32 RawItemCount = 0;
		for (const FArcRecipeGraphNodeData& Node : Data.Nodes)
		{
			if (Node.Type == EArcRecipeGraphNodeType::Recipe)  { ++RecipeCount;  }
			if (Node.Type == EArcRecipeGraphNodeType::RawItem) { ++RawItemCount; }
		}
		ASSERT_THAT(AreEqual(1, RecipeCount,  TEXT("(b) 1 Recipe node expected")));
		ASSERT_THAT(AreEqual(1, RawItemCount, TEXT("(b) 1 RawItem node expected")));

		// Must have 1 edge: ItemRaw -> RecipeC
		bool bEdgeFound = false;
		for (const FArcRecipeGraphEdgeData& Edge : Data.Edges)
		{
			if (Edge.FromNode == ItemRaw.Id &&
				Edge.ToNode   == RecipeC.Id)
			{
				bEdgeFound = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bEdgeFound, TEXT("(b) RawItem->Recipe edge not found")));
	}

	// =========================================================================
	// Case (c): tag ingredient with only raw item matches — one TagGroup node
	//
	//   IronOre has AssetTags = {Metal.Iron}, not produced by any recipe
	//   GoldOre has AssetTags = {Metal.Iron, Metal.Gold}, not produced
	//   RecipeD wants tags HasAll({Metal.Iron}), no Deny
	//   => 1 TagGroup node, 1 TagGroup->RecipeD edge
	// =========================================================================
	TEST_METHOD(TagIngredient_OnlyRaw_OneTagGroupNode)
	{
		using namespace ArcRecipeGraphBuilderTestHelpers;

		FGameplayTagContainer IronTags;
		IronTags.AddTag(TAG_GraphTest_Metal_Iron);

		FGameplayTagContainer GoldIronTags;
		GoldIronTags.AddTag(TAG_GraphTest_Metal_Iron);
		GoldIronTags.AddTag(TAG_GraphTest_Metal_Gold);

		const FGraphTestItem IronOre  = MakeItem(TEXT("GT_IronOre"),  ItemAssetType, IronTags);
		const FGraphTestItem GoldOre  = MakeItem(TEXT("GT_GoldOre"),  ItemAssetType, GoldIronTags);
		const FGraphTestItem OutputD  = MakeItem(TEXT("GT_OutputD"),  ItemAssetType, FGameplayTagContainer{});

		FGraphTestRecipe RecipeD = MakeRecipe(TEXT("GT_RecipeD_TagRaw"), RecipeAssetType, OutputD);

		FGameplayTagContainer Required;
		Required.AddTag(TAG_GraphTest_Metal_Iron);
		AddTagIngredient(RecipeD, Required, FGameplayTagContainer{}, 3, 0.5f);

		TArray<FArcRecipeGraphRecipe> Recipes = { MakeGraphRecipe(RecipeD) };
		TArray<FArcRecipeGraphItem> Items = { MakeGraphItem(IronOre), MakeGraphItem(GoldOre), MakeGraphItem(OutputD) };

		FArcRecipeGraphData Data = FArcRecipeGraphBuilder::BuildFromRecords(Recipes, Items);

		int32 TagGroupCount = 0;
		for (const FArcRecipeGraphNodeData& Node : Data.Nodes)
		{
			if (Node.Type == EArcRecipeGraphNodeType::TagGroup) { ++TagGroupCount; }
		}
		ASSERT_THAT(AreEqual(1, TagGroupCount, TEXT("(c) exactly 1 TagGroup node expected")));

		// Edge: TagGroup -> RecipeD
		bool bEdgeFound = false;
		for (const FArcRecipeGraphEdgeData& Edge : Data.Edges)
		{
			if (Edge.ToNode == RecipeD.Id &&
				Edge.MinRarityStep > 0.0f)
			{
				bEdgeFound = true;
				ASSERT_THAT(AreEqual(3, Edge.Amount, TEXT("(c) edge amount must be 3")));
				break;
			}
		}
		ASSERT_THAT(IsTrue(bEdgeFound, TEXT("(c) TagGroup->Recipe edge not found")));
	}

	// =========================================================================
	// Case (d): tag ingredient mixing crafted + raw — crafted gets direct edges,
	//           raw remainder collapses to one TagGroup node
	//
	//   IronBar is produced by RecipeE (smelting)
	//   WoodPlank is raw
	//   Both have AssetTag {Metal.Iron} (contrived for test)
	//   RecipeF wants HasAll({Metal.Iron})
	//   => direct RecipeE->RecipeF edge + 1 TagGroup node for WoodPlank
	// =========================================================================
	TEST_METHOD(TagIngredient_MixedCraftedAndRaw_SplitEdges)
	{
		using namespace ArcRecipeGraphBuilderTestHelpers;

		FGameplayTagContainer MetalTag;
		MetalTag.AddTag(TAG_GraphTest_Metal_Iron);

		const FGraphTestItem IronBar   = MakeItem(TEXT("GT_IronBar"),   ItemAssetType, MetalTag);
		const FGraphTestItem WoodPlank = MakeItem(TEXT("GT_WoodPlank"), ItemAssetType, MetalTag);
		const FGraphTestItem OutputE   = MakeItem(TEXT("GT_OutputE"),   ItemAssetType, FGameplayTagContainer{});
		const FGraphTestItem OutputF   = MakeItem(TEXT("GT_OutputF"),   ItemAssetType, FGameplayTagContainer{});

		// RecipeE produces IronBar
		FGraphTestRecipe RecipeE = MakeRecipe(TEXT("GT_RecipeE_Smelt"), RecipeAssetType, IronBar);

		// RecipeF wants anything with Metal.Iron (mixed: IronBar crafted, WoodPlank raw)
		FGraphTestRecipe RecipeF = MakeRecipe(TEXT("GT_RecipeF_Consumer"), RecipeAssetType, OutputF);
		FGameplayTagContainer Required;
		Required.AddTag(TAG_GraphTest_Metal_Iron);
		AddTagIngredient(RecipeF, Required, FGameplayTagContainer{}, 1, 0.0f);

		TArray<FArcRecipeGraphRecipe> Recipes = { MakeGraphRecipe(RecipeE), MakeGraphRecipe(RecipeF) };
		TArray<FArcRecipeGraphItem> Items = { MakeGraphItem(IronBar), MakeGraphItem(WoodPlank), MakeGraphItem(OutputE), MakeGraphItem(OutputF) };

		FArcRecipeGraphData Data = FArcRecipeGraphBuilder::BuildFromRecords(Recipes, Items);

		// Must have exactly 1 TagGroup node (for WoodPlank)
		int32 TagGroupCount = 0;
		for (const FArcRecipeGraphNodeData& Node : Data.Nodes)
		{
			if (Node.Type == EArcRecipeGraphNodeType::TagGroup) { ++TagGroupCount; }
		}
		ASSERT_THAT(AreEqual(1, TagGroupCount, TEXT("(d) exactly 1 TagGroup for WoodPlank expected")));

		// Must have a direct RecipeE -> RecipeF edge
		bool bDirectEdge = false;
		for (const FArcRecipeGraphEdgeData& Edge : Data.Edges)
		{
			if (Edge.FromNode == RecipeE.Id &&
				Edge.ToNode   == RecipeF.Id)
			{
				bDirectEdge = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bDirectEdge, TEXT("(d) direct RecipeE->RecipeF edge expected")));

		// Must also have a TagGroup -> RecipeF edge
		bool bGroupEdge = false;
		for (const FArcRecipeGraphEdgeData& Edge : Data.Edges)
		{
			if (Edge.ToNode == RecipeF.Id &&
				Edge.FromNode != RecipeE.Id)
			{
				bGroupEdge = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bGroupEdge, TEXT("(d) TagGroup->RecipeF edge expected")));
	}

	// =========================================================================
	// Case (e): BuildFocused returns only root's up/downstream, cycle-safe
	//
	//   RecipeG produces ItemG, RecipeH consumes ItemG and produces ItemH,
	//   RecipeI consumes ItemH and produces ItemG (cycle: G<->H<->I<->G)
	//   RecipeJ is unrelated
	//   Focus on RecipeG => visits G, H, I but NOT J
	// =========================================================================
	TEST_METHOD(BuildFocused_UpDownstream_CycleGuard)
	{
		using namespace ArcRecipeGraphBuilderTestHelpers;

		const FGraphTestItem ItemG = MakeItem(TEXT("GT_ItemG"), ItemAssetType, FGameplayTagContainer{});
		const FGraphTestItem ItemH = MakeItem(TEXT("GT_ItemH"), ItemAssetType, FGameplayTagContainer{});
		const FGraphTestItem ItemI = MakeItem(TEXT("GT_ItemI"), ItemAssetType, FGameplayTagContainer{});
		const FGraphTestItem ItemJ = MakeItem(TEXT("GT_ItemJ"), ItemAssetType, FGameplayTagContainer{});

		// RecipeG: produces ItemG, no ingredients
		FGraphTestRecipe RecipeG = MakeRecipe(TEXT("GT_RecipeG"), RecipeAssetType, ItemG);

		// RecipeH: produces ItemH, consumes ItemG
		FGraphTestRecipe RecipeH = MakeRecipe(TEXT("GT_RecipeH"), RecipeAssetType, ItemH);
		AddItemDefIngredient(RecipeH, ItemG, 1, true);

		// RecipeI: produces ItemG (cycle!), consumes ItemH
		FGraphTestRecipe RecipeI = MakeRecipe(TEXT("GT_RecipeI"), RecipeAssetType, ItemG);
		AddItemDefIngredient(RecipeI, ItemH, 1, true);

		// RecipeJ: unrelated
		FGraphTestRecipe RecipeJ = MakeRecipe(TEXT("GT_RecipeJ"), RecipeAssetType, ItemJ);

		TArray<FArcRecipeGraphRecipe> Recipes = { MakeGraphRecipe(RecipeG), MakeGraphRecipe(RecipeH), MakeGraphRecipe(RecipeI), MakeGraphRecipe(RecipeJ) };
		TArray<FArcRecipeGraphItem> Items = { MakeGraphItem(ItemG), MakeGraphItem(ItemH), MakeGraphItem(ItemI), MakeGraphItem(ItemJ) };

		FArcRecipeGraphData Full = FArcRecipeGraphBuilder::BuildFromRecords(Recipes, Items);

		// Focus on RecipeG
		FArcRecipeGraphData Focused = FArcRecipeGraphBuilder::BuildFocused(
			Full, RecipeG.Id);

		// RecipeJ must not appear
		bool bJFound = false;
		for (const FArcRecipeGraphNodeData& Node : Focused.Nodes)
		{
			if (Node.AssetId == RecipeJ.Id)
			{
				bJFound = true;
				break;
			}
		}
		ASSERT_THAT(IsFalse(bJFound, TEXT("(e) RecipeJ must not appear in focused subgraph")));

		// RecipeG, RecipeH, RecipeI must all appear (cycle traversed safely)
		bool bGFound = false;
		bool bHFound = false;
		bool bIFound = false;
		for (const FArcRecipeGraphNodeData& Node : Focused.Nodes)
		{
			if (Node.AssetId == RecipeG.Id) { bGFound = true; }
			if (Node.AssetId == RecipeH.Id) { bHFound = true; }
			if (Node.AssetId == RecipeI.Id) { bIFound = true; }
		}
		ASSERT_THAT(IsTrue(bGFound, TEXT("(e) RecipeG must appear in focused subgraph")));
		ASSERT_THAT(IsTrue(bHFound, TEXT("(e) RecipeH must appear in focused subgraph")));
		ASSERT_THAT(IsTrue(bIFound, TEXT("(e) RecipeI must appear in focused subgraph")));
	}

	// =========================================================================
	// Case (f): BuildFocused is strict ancestors+descendants, NOT a flood.
	//
	//   RootR produces ItemR; RecipeDown consumes ItemR (so RootR -> RecipeDown).
	//   RecipeCousin produces ItemCousin, which RecipeDown ALSO consumes.
	//   RecipeCousin is an ancestor of RecipeDown but is neither an ancestor nor
	//   a descendant of RootR. Focusing on RootR must therefore EXCLUDE
	//   RecipeCousin (a bidirectional flood would have wrongly included it).
	// =========================================================================
	TEST_METHOD(BuildFocused_ExcludesCousins)
	{
		using namespace ArcRecipeGraphBuilderTestHelpers;

		const FGraphTestItem ItemR      = MakeItem(TEXT("GT_ItemR"),      ItemAssetType, FGameplayTagContainer{});
		const FGraphTestItem ItemDown   = MakeItem(TEXT("GT_ItemDown"),   ItemAssetType, FGameplayTagContainer{});
		const FGraphTestItem ItemCousin = MakeItem(TEXT("GT_ItemCousin"), ItemAssetType, FGameplayTagContainer{});

		FGraphTestRecipe RootR = MakeRecipe(TEXT("GT_RootR"), RecipeAssetType, ItemR);

		FGraphTestRecipe RecipeCousin = MakeRecipe(TEXT("GT_RecipeCousin"), RecipeAssetType, ItemCousin);

		// RecipeDown consumes BOTH ItemR (from RootR) and ItemCousin (from RecipeCousin).
		FGraphTestRecipe RecipeDown = MakeRecipe(TEXT("GT_RecipeDown"), RecipeAssetType, ItemDown);
		AddItemDefIngredient(RecipeDown, ItemR,      1, true);
		AddItemDefIngredient(RecipeDown, ItemCousin, 1, true);

		TArray<FArcRecipeGraphRecipe> Recipes = { MakeGraphRecipe(RootR), MakeGraphRecipe(RecipeCousin), MakeGraphRecipe(RecipeDown) };
		TArray<FArcRecipeGraphItem> Items = { MakeGraphItem(ItemR), MakeGraphItem(ItemDown), MakeGraphItem(ItemCousin) };

		FArcRecipeGraphData Full = FArcRecipeGraphBuilder::BuildFromRecords(Recipes, Items);
		FArcRecipeGraphData Focused = FArcRecipeGraphBuilder::BuildFocused(
			Full, RootR.Id);

		bool bRootFound   = false;
		bool bDownFound   = false;
		bool bCousinFound = false;
		for (const FArcRecipeGraphNodeData& Node : Focused.Nodes)
		{
			if (Node.AssetId == RootR.Id)        { bRootFound = true;   }
			if (Node.AssetId == RecipeDown.Id)   { bDownFound = true;   }
			if (Node.AssetId == RecipeCousin.Id) { bCousinFound = true; }
		}
		ASSERT_THAT(IsTrue(bRootFound,  TEXT("(f) RootR (focus root) must appear")));
		ASSERT_THAT(IsTrue(bDownFound,  TEXT("(f) RecipeDown (descendant) must appear")));
		ASSERT_THAT(IsFalse(bCousinFound, TEXT("(f) RecipeCousin (cousin) must NOT appear")));
	}
};

#endif // WITH_EDITOR
