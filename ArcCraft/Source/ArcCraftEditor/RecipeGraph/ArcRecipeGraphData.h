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

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"

/**
 * The three kinds of nodes in the recipe dependency graph.
 *
 * Recipe    — one node per recipe record.
 * RawItem   — one node per item record that no recipe produces.
 * TagGroup  — one node per tag-based ingredient's raw candidate set.
 */
UENUM()
enum class EArcRecipeGraphNodeType : uint8
{
	Recipe,
	RawItem,
	TagGroup,
};

/**
 * Plain-old-data description of one graph node.
 *
 * For Recipe and RawItem nodes AssetId uniquely identifies the underlying asset.
 * For TagGroup nodes AssetId is a synthesized stable key derived from the sorted
 * RequiredTags string; GroupTags and GroupMembers carry the actual tag set and
 * the raw item ids that belong to the group.
 */
struct FArcRecipeGraphNodeData
{
	EArcRecipeGraphNodeType Type = EArcRecipeGraphNodeType::Recipe;

	/** Stable identity key (recipe asset id, item asset id, or synthesised tag-group key). */
	FPrimaryAssetId AssetId;

	/** Display label shown in the graph node title. */
	FText Title;

	/** For TagGroup: the RequiredTags that define the group. Empty for other types. */
	FGameplayTagContainer GroupTags;

	/** For TagGroup: the raw item asset ids that are members of this group. Empty for other types. */
	TArray<FPrimaryAssetId> GroupMembers;

	/** For TagGroup: display titles parallel to GroupMembers. */
	TArray<FText> GroupMemberTitles;
};

/**
 * Plain-old-data description of one directed edge in the recipe dependency graph.
 *
 * Direction is always producer -> consumer (output -> input):
 *   RawItem(out)  -> Recipe(in)
 *   TagGroup(out) -> Recipe(in)
 *   Recipe(out)   -> Recipe(in)   (when the upstream recipe produces what the downstream consumes)
 *
 * Edge annotations come directly from the FArcRecipeIngredient base fields.
 */
struct FArcRecipeGraphEdgeData
{
	FPrimaryAssetId FromNode;
	FPrimaryAssetId ToNode;

	/** Number of this ingredient required (from FArcRecipeIngredient::Amount). */
	int32 Amount = 1;

	/** Whether this ingredient is consumed on craft (catalyst = false). */
	bool bConsumeOnCraft = true;

	/** Minimum rarity step required (from FArcRecipeIngredient::MinRarityStep). */
	float MinRarityStep = 0.0f;

	/** Ingredient may be left empty (from FArcRecipeIngredient::bOptional). */
	bool bOptional = false;
};

/**
 * The complete pure-data representation of the recipe dependency graph.
 * Built by FArcRecipeGraphBuilder; consumed by PopulateGraph and the test suite.
 */
struct FArcRecipeGraphData
{
	TArray<FArcRecipeGraphNodeData> Nodes;
	TArray<FArcRecipeGraphEdgeData> Edges;
};
