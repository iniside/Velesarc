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

#include "RecipeGraph/ArcRecipeGraphBuilder.h"

#include "RecipeGraph/ArcRecipeGraph.h"
#include "RecipeGraph/ArcRecipeGraphNode.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "Core/ArcCoreAssetManager.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "GameplayTagContainer.h"

// ---------------------------------------------------------------------------
// File-scoped helper namespace (unique across the project)
// ---------------------------------------------------------------------------

namespace ArcRecipeGraphBuilderInternal
{
	/**
	 * Return a stable FPrimaryAssetId key for a tag-group node.
	 * Key is synthesised from the sorted RequiredTags string so that the same
	 * ingredient produces the same key regardless of construction order.
	 */
	static FPrimaryAssetId MakeTagGroupKey(const FGameplayTagContainer& RequiredTags)
	{
		TArray<FString> TagStrings;
		TagStrings.Reserve(RequiredTags.Num());
		for (const FGameplayTag& Tag : RequiredTags)
		{
			TagStrings.Add(Tag.ToString());
		}
		TagStrings.Sort();

		FString Combined;
		for (const FString& S : TagStrings)
		{
			if (!Combined.IsEmpty())
			{
				Combined += TEXT("+");
			}
			Combined += S;
		}

		// Use a synthetic asset type so these keys never collide with real assets.
		return FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcRecipeTagGroup")), FName(*Combined));
	}

	const TCHAR* RecipeExtension = TEXT("arcrecipe");

	/**
	 * Collect item display title.  Falls back to the record file name when no
	 * FArcItemFragment_UIDescription is present.
	 */
	static FText GetItemTitle(const FArcRecipeGraphItem& Entry)
	{
		const FArcItemFragment_UIDescription* UIDesc = Entry.Item != nullptr ? Entry.Item->FindFragment<FArcItemFragment_UIDescription>() : nullptr;
		if (UIDesc != nullptr && !UIDesc->ItemName.IsEmpty())
		{
			return UIDesc->ItemName;
		}
		return Entry.FallbackTitle;
	}

	/**
	 * Ensure a node with the given key exists in OutData.
	 * No-op if the key is already present.
	 */
	static void EnsureNode(FArcRecipeGraphData& OutData, const FArcRecipeGraphNodeData& NodeData)
	{
		for (const FArcRecipeGraphNodeData& Existing : OutData.Nodes)
		{
			if (Existing.AssetId == NodeData.AssetId)
			{
				return;
			}
		}
		OutData.Nodes.Add(NodeData);
	}

	/**
	 * Ensure a directed edge exists in OutData.
	 * Deduplication key is (FromNode, ToNode) — amount/flags are taken from the
	 * first occurrence (earliest ingredient wins);
	 * the edge stays optional only while every ingredient behind it is optional.
	 */
	static void EnsureEdge(FArcRecipeGraphData& OutData, const FArcRecipeGraphEdgeData& EdgeData)
	{
		for (FArcRecipeGraphEdgeData& Existing : OutData.Edges)
		{
			if (Existing.FromNode == EdgeData.FromNode && Existing.ToNode == EdgeData.ToNode)
			{
				Existing.bOptional = Existing.bOptional && EdgeData.bOptional;
				return;
			}
		}
		OutData.Edges.Add(EdgeData);
	}

} // namespace ArcRecipeGraphBuilderInternal

// ---------------------------------------------------------------------------
// BuildFromRecords
// ---------------------------------------------------------------------------

FArcRecipeGraphData FArcRecipeGraphBuilder::BuildFromRecords(
	TArrayView<const FArcRecipeGraphRecipe> Recipes,
	TArrayView<const FArcRecipeGraphItem> Items)
{
	FArcRecipeGraphData OutData;

	// ---- Pass 1: build lookup maps -------------------------------------------

	// ItemId -> list of recipe ids that produce that item
	TMap<FPrimaryAssetId, TArray<FPrimaryAssetId>> ItemToProducingRecipes;

	// ItemId -> AssetTags from its FArcItemFragment_Tags
	TMap<FPrimaryAssetId, FGameplayTagContainer> ItemToAssetTags;

	for (const FArcRecipeGraphRecipe& Entry : Recipes)
	{
		if (Entry.Recipe == nullptr)
		{
			continue;
		}
		for (const FArcRecipeOutput& Output : Entry.Recipe->Outputs)
		{
			const FPrimaryAssetId OutputItemId = Output.ItemDefinition.AssetId;
			if (!OutputItemId.IsValid())
			{
				continue;
			}
			TArray<FPrimaryAssetId>& ProducingList = ItemToProducingRecipes.FindOrAdd(OutputItemId);
			ProducingList.AddUnique(Entry.Id);
		}
	}

	for (const FArcRecipeGraphItem& ItemEntry : Items)
	{
		if (ItemEntry.Item == nullptr)
		{
			continue;
		}
		const FPrimaryAssetId ItemId = ItemEntry.Id;
		if (!ItemId.IsValid())
		{
			continue;
		}
		const FArcItemFragment_Tags* TagsFrag = ItemEntry.Item->FindFragment<FArcItemFragment_Tags>();
		if (TagsFrag != nullptr)
		{
			ItemToAssetTags.Add(ItemId, TagsFrag->AssetTags);
		}
		else
		{
			// Item has no tags fragment — add with an empty container so it still
			// participates in exact-id matching.
			ItemToAssetTags.Add(ItemId, FGameplayTagContainer());
		}
	}

	// ---- Pass 2: emit Recipe nodes -------------------------------------------

	for (const FArcRecipeGraphRecipe& Entry : Recipes)
	{
		if (Entry.Recipe == nullptr)
		{
			continue;
		}
		FArcRecipeGraphNodeData RecipeNode;
		RecipeNode.Type    = EArcRecipeGraphNodeType::Recipe;
		RecipeNode.AssetId = Entry.Id;
		const FText RecipeDisplayName = Entry.Recipe->GetDisplayName();
		RecipeNode.Title   = RecipeDisplayName.IsEmpty()
		                         ? Entry.FallbackTitle
		                         : RecipeDisplayName;

		ArcRecipeGraphBuilderInternal::EnsureNode(OutData, RecipeNode);
	}

	// ---- Pass 3: process each recipe's ingredients ---------------------------

	for (const FArcRecipeGraphRecipe& Entry : Recipes)
	{
		const FArcRecipeDefinitionData* Recipe = Entry.Recipe;
		if (Recipe == nullptr)
		{
			continue;
		}
		const FPrimaryAssetId RecipeId = Entry.Id;
		const int32 IngCount           = Recipe->GetIngredientCount();

		for (int32 IngIdx = 0; IngIdx < IngCount; ++IngIdx)
		{
			const FArcRecipeIngredient* Base = Recipe->GetIngredientBase(IngIdx);
			if (Base == nullptr)
			{
				continue;
			}

			// --- Sub-case A: specific-item ingredient ---
			const FArcRecipeIngredient_ItemDef* ItemDefIng =
				Recipe->GetIngredient<FArcRecipeIngredient_ItemDef>(IngIdx);
			if (ItemDefIng != nullptr)
			{
				const FPrimaryAssetId IngItemId = ItemDefIng->ItemDefinitionId.AssetId;
				if (!IngItemId.IsValid())
				{
					continue;
				}

				const TArray<FPrimaryAssetId>* ProducingRecipes =
					ItemToProducingRecipes.Find(IngItemId);

				if (ProducingRecipes != nullptr && ProducingRecipes->Num() > 0)
				{
					// Crafted item: draw recipe->recipe edges
					for (const FPrimaryAssetId& ProducerRecipeId : *ProducingRecipes)
					{
						FArcRecipeGraphEdgeData Edge;
						Edge.FromNode       = ProducerRecipeId;
						Edge.ToNode         = RecipeId;
						Edge.Amount         = Base->Amount;
						Edge.bConsumeOnCraft = Base->bConsumeOnCraft;
						Edge.MinRarityStep     = Base->MinRarityStep;
						Edge.bOptional      = Base->bOptional;
						ArcRecipeGraphBuilderInternal::EnsureEdge(OutData, Edge);
					}
				}
				else
				{
					// Raw item: emit RawItem node and edge from it
					FArcRecipeGraphNodeData RawNode;
					RawNode.Type    = EArcRecipeGraphNodeType::RawItem;
					RawNode.AssetId = IngItemId;

					// Look up title from Items array
					FText RawTitle = FText::FromString(IngItemId.ToString());
					for (const FArcRecipeGraphItem& ItemEntry : Items)
					{
						if (ItemEntry.Item != nullptr && ItemEntry.Id == IngItemId)
						{
							RawTitle = ArcRecipeGraphBuilderInternal::GetItemTitle(ItemEntry);
							break;
						}
					}
					RawNode.Title = RawTitle;
					ArcRecipeGraphBuilderInternal::EnsureNode(OutData, RawNode);

					FArcRecipeGraphEdgeData Edge;
					Edge.FromNode       = IngItemId;
					Edge.ToNode         = RecipeId;
					Edge.Amount         = Base->Amount;
					Edge.bConsumeOnCraft = Base->bConsumeOnCraft;
					Edge.MinRarityStep     = Base->MinRarityStep;
					Edge.bOptional      = Base->bOptional;
					ArcRecipeGraphBuilderInternal::EnsureEdge(OutData, Edge);
				}
				continue;
			}

			// --- Sub-case B: tag-based ingredient ---
			const FArcRecipeIngredient_Tags* TagIng =
				Recipe->GetIngredient<FArcRecipeIngredient_Tags>(IngIdx);
			if (TagIng == nullptr)
			{
				// Unknown ingredient subtype — skip.
				continue;
			}

			const FGameplayTagContainer& RequiredTags = TagIng->RequiredTags;
			const FGameplayTagContainer& DenyTags      = TagIng->DenyTags;

			// Collect candidates: items whose AssetTags satisfy the tag filter
			TArray<FPrimaryAssetId> CraftedCandidates;
			TArray<FPrimaryAssetId> RawCandidates;

			for (const TPair<FPrimaryAssetId, FGameplayTagContainer>& Pair : ItemToAssetTags)
			{
				const FGameplayTagContainer& AssetTags = Pair.Value;
				bool bPassesRequired = RequiredTags.IsEmpty() || AssetTags.HasAll(RequiredTags);
				bool bPassesDeny     = DenyTags.IsEmpty()     || !AssetTags.HasAny(DenyTags);

				if (!bPassesRequired || !bPassesDeny)
				{
					continue;
				}

				const TArray<FPrimaryAssetId>* Producers = ItemToProducingRecipes.Find(Pair.Key);
				if (Producers != nullptr && Producers->Num() > 0)
				{
					CraftedCandidates.Add(Pair.Key);
				}
				else
				{
					RawCandidates.Add(Pair.Key);
				}
			}

			// Crafted matches -> direct recipe->recipe edges (dedup per producing recipe)
			TSet<FPrimaryAssetId> EmittedProducerIds;
			for (const FPrimaryAssetId& CraftedItemId : CraftedCandidates)
			{
				const TArray<FPrimaryAssetId>* Producers = ItemToProducingRecipes.Find(CraftedItemId);
				if (Producers == nullptr)
				{
					continue;
				}
				for (const FPrimaryAssetId& ProducerRecipeId : *Producers)
				{
					if (EmittedProducerIds.Contains(ProducerRecipeId))
					{
						continue;
					}
					EmittedProducerIds.Add(ProducerRecipeId);

					FArcRecipeGraphEdgeData Edge;
					Edge.FromNode       = ProducerRecipeId;
					Edge.ToNode         = RecipeId;
					Edge.Amount         = Base->Amount;
					Edge.bConsumeOnCraft = Base->bConsumeOnCraft;
					Edge.MinRarityStep     = Base->MinRarityStep;
					Edge.bOptional      = Base->bOptional;
					ArcRecipeGraphBuilderInternal::EnsureEdge(OutData, Edge);
				}
			}

			// Raw matches -> one TagGroup node + edge
			if (RawCandidates.Num() > 0)
			{
				const FPrimaryAssetId GroupKey =
					ArcRecipeGraphBuilderInternal::MakeTagGroupKey(RequiredTags);

				FArcRecipeGraphNodeData GroupNode;
				GroupNode.Type        = EArcRecipeGraphNodeType::TagGroup;
				GroupNode.AssetId     = GroupKey;
				GroupNode.GroupTags   = RequiredTags;
				GroupNode.GroupMembers = RawCandidates;

				GroupNode.GroupMemberTitles.Reserve(RawCandidates.Num());
				for (const FPrimaryAssetId& MemberId : RawCandidates)
				{
					FText MemberTitle = FText::FromName(MemberId.PrimaryAssetName);
					for (const FArcRecipeGraphItem& ItemEntry : Items)
					{
						if (ItemEntry.Item != nullptr && ItemEntry.Id == MemberId)
						{
							MemberTitle = ArcRecipeGraphBuilderInternal::GetItemTitle(ItemEntry);
							break;
						}
					}
					GroupNode.GroupMemberTitles.Add(MemberTitle);
				}

				// Title = sorted tag list
				TArray<FString> TagStrings;
				TagStrings.Reserve(RequiredTags.Num());
				for (const FGameplayTag& Tag : RequiredTags)
				{
					TagStrings.Add(Tag.ToString());
				}
				TagStrings.Sort();
				GroupNode.Title = FText::FromString(FString::Join(TagStrings, TEXT(", ")));

				ArcRecipeGraphBuilderInternal::EnsureNode(OutData, GroupNode);

				FArcRecipeGraphEdgeData GroupEdge;
				GroupEdge.FromNode       = GroupKey;
				GroupEdge.ToNode         = RecipeId;
				GroupEdge.Amount         = Base->Amount;
				GroupEdge.bConsumeOnCraft = Base->bConsumeOnCraft;
				GroupEdge.MinRarityStep     = Base->MinRarityStep;
				GroupEdge.bOptional      = Base->bOptional;
				ArcRecipeGraphBuilderInternal::EnsureEdge(OutData, GroupEdge);
			}
		}
	}

	return OutData;
}

// ---------------------------------------------------------------------------
// BuildFull
// ---------------------------------------------------------------------------

FArcRecipeGraphData FArcRecipeGraphBuilder::BuildFull()
{
	UArcCoreAssetManager& AssetManager = UArcCoreAssetManager::Get();
	const FString ItemExtension = FArcItemDefinitionData::JsonExtension();

	TArray<FArcRecipeGraphRecipe> RecipeEntries;
	TArray<FArcRecipeGraphItem> ItemEntries;
	for (const FPrimaryAssetType& Type : AssetManager.GetJsonRecordTypes())
	{
		const FArcJsonAssetTypeInfo* TypeInfo = AssetManager.FindJsonRecordType(Type);
		if (TypeInfo == nullptr)
		{
			continue;
		}
		if (TypeInfo->Extension == ArcRecipeGraphBuilderInternal::RecipeExtension)
		{
			for (const FArcJsonRecordEntry& RecordEntry : AssetManager.GetJsonRecordEntries(Type))
			{
				const FArcRecipeDefinitionData* Recipe = AssetManager.FindJsonRecord<FArcRecipeDefinitionData>(RecordEntry.Id);
				if (Recipe != nullptr)
				{
					RecipeEntries.Add({ RecordEntry.Id, FText::FromString(RecordEntry.DisplayName), Recipe });
				}
			}
		}
		else if (TypeInfo->Extension == ItemExtension)
		{
			for (const FArcJsonRecordEntry& RecordEntry : AssetManager.GetJsonRecordEntries(Type))
			{
				const FArcItemDefinitionData* Item = AssetManager.FindJsonRecord<FArcItemDefinitionData>(RecordEntry.Id);
				if (Item != nullptr)
				{
					ItemEntries.Add({ RecordEntry.Id, FText::FromString(RecordEntry.DisplayName), Item });
				}
			}
		}
	}

	return BuildFromRecords(RecipeEntries, ItemEntries);
}

// ---------------------------------------------------------------------------
// BuildFocused
// ---------------------------------------------------------------------------

FArcRecipeGraphData FArcRecipeGraphBuilder::BuildFocused(
	const FArcRecipeGraphData& Full,
	FPrimaryAssetId RootRecipe)
{
	FArcRecipeGraphData OutData;

	// Verify the root exists.
	bool bRootFound = false;
	for (const FArcRecipeGraphNodeData& Node : Full.Nodes)
	{
		if (Node.AssetId == RootRecipe)
		{
			bRootFound = true;
			break;
		}
	}
	if (!bRootFound)
	{
		return OutData;
	}

	// The focused subgraph is the root's strict transitive ancestors (what it
	// requires) UNION its strict transitive descendants (what it enables) — NOT a
	// bidirectional flood, which would also pull in the sibling inputs of
	// downstream recipes ("cousins"). Each direction is walked independently and
	// only ever continues in its own direction; a per-walk visited set guards cycles.
	TSet<FPrimaryAssetId> Visited;
	Visited.Add(RootRecipe);

	// Upstream walk: follow edges where current is the consumer (ToNode), collect producers.
	{
		TArray<FPrimaryAssetId> Frontier;
		Frontier.Add(RootRecipe);
		while (Frontier.Num() > 0)
		{
			TArray<FPrimaryAssetId> NextFrontier;
			for (const FPrimaryAssetId& CurrentId : Frontier)
			{
				for (const FArcRecipeGraphEdgeData& Edge : Full.Edges)
				{
					if (Edge.ToNode == CurrentId && !Visited.Contains(Edge.FromNode))
					{
						Visited.Add(Edge.FromNode);
						NextFrontier.Add(Edge.FromNode);
					}
				}
			}
			Frontier = MoveTemp(NextFrontier);
		}
	}

	// Downstream walk: follow edges where current is the producer (FromNode), collect consumers.
	{
		TArray<FPrimaryAssetId> Frontier;
		Frontier.Add(RootRecipe);
		while (Frontier.Num() > 0)
		{
			TArray<FPrimaryAssetId> NextFrontier;
			for (const FPrimaryAssetId& CurrentId : Frontier)
			{
				for (const FArcRecipeGraphEdgeData& Edge : Full.Edges)
				{
					if (Edge.FromNode == CurrentId && !Visited.Contains(Edge.ToNode))
					{
						Visited.Add(Edge.ToNode);
						NextFrontier.Add(Edge.ToNode);
					}
				}
			}
			Frontier = MoveTemp(NextFrontier);
		}
	}

	// Collect induced nodes.
	for (const FArcRecipeGraphNodeData& Node : Full.Nodes)
	{
		if (Visited.Contains(Node.AssetId))
		{
			OutData.Nodes.Add(Node);
		}
	}

	// Collect induced edges (both endpoints must be in the visited set).
	for (const FArcRecipeGraphEdgeData& Edge : Full.Edges)
	{
		if (Visited.Contains(Edge.FromNode) && Visited.Contains(Edge.ToNode))
		{
			OutData.Edges.Add(Edge);
		}
	}

	return OutData;
}

// ---------------------------------------------------------------------------
// PopulateGraph
// ---------------------------------------------------------------------------

void FArcRecipeGraphBuilder::PopulateGraph(UArcRecipeGraph& Graph, const FArcRecipeGraphData& Data)
{
	// Remove all existing nodes so the graph is ready for a clean repopulation.
	Graph.Nodes.Empty();

	// Build a map from asset id to the freshly created UEdGraphNode so we can
	// wire edges after all nodes are in place.
	TMap<FPrimaryAssetId, UArcRecipeGraphNode*> NodeMap;
	NodeMap.Reserve(Data.Nodes.Num());

	for (const FArcRecipeGraphNodeData& NodeData : Data.Nodes)
	{
		UArcRecipeGraphNode* Node = NewObject<UArcRecipeGraphNode>(&Graph);
		Node->NodeType     = NodeData.Type;
		Node->AssetId      = NodeData.AssetId;
		Node->GroupTags    = NodeData.GroupTags;
		Node->GroupMembers = NodeData.GroupMembers;
		Node->GroupMemberTitles = NodeData.GroupMemberTitles;
		Node->DisplayTitle = NodeData.Title;

		Node->AllocateDefaultPins();

		// AddNode registers the node without triggering user-action or selection.
		Graph.AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);

		NodeMap.Add(NodeData.AssetId, Node);
	}

	// Wire edges: producer output pin -> consumer input pin.
	Graph.OptionalEdges.Reset();
	for (const FArcRecipeGraphEdgeData& EdgeData : Data.Edges)
	{
		UArcRecipeGraphNode* const* FromPtr = NodeMap.Find(EdgeData.FromNode);
		UArcRecipeGraphNode* const* ToPtr   = NodeMap.Find(EdgeData.ToNode);

		if (FromPtr == nullptr || ToPtr == nullptr)
		{
			continue;
		}

		UArcRecipeGraphNode* FromNode = *FromPtr;
		UArcRecipeGraphNode* ToNode   = *ToPtr;

		if (FromNode == nullptr || ToNode == nullptr)
		{
			continue;
		}

		UEdGraphPin* OutputPin = FromNode->GetOutputPin();
		UEdGraphPin* InputPin  = ToNode->GetInputPin();

		if (OutputPin == nullptr || InputPin == nullptr)
		{
			continue;
		}

		OutputPin->MakeLinkTo(InputPin);
		if (EdgeData.bOptional)
		{
			Graph.OptionalEdges.Add(MakeTuple(EdgeData.FromNode, EdgeData.ToNode));
		}
	}
}
