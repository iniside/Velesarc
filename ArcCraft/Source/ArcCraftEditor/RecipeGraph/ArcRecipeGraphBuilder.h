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
#include "RecipeGraph/ArcRecipeGraphData.h"

class UArcRecipeGraph;
struct FArcItemDefinitionData;
struct FArcRecipeDefinitionData;

struct FArcRecipeGraphRecipe
{
	FPrimaryAssetId Id;
	FText FallbackTitle;
	const FArcRecipeDefinitionData* Recipe = nullptr;
};

struct FArcRecipeGraphItem
{
	FPrimaryAssetId Id;
	FText FallbackTitle;
	const FArcItemDefinitionData* Item = nullptr;
};

/**
 * Static builder that produces FArcRecipeGraphData from recipe and item records.
 *
 * Three entry points:
 *
 *  BuildFromRecords — pure, no AssetRegistry; used directly by tests and internally
 *                     by the other two entry points.
 *
 *  BuildFull        — enumerates every indexed .arcrecipe and .arcitem record,
 *                     then calls BuildFromRecords.
 *
 *  BuildFocused     — induces a subgraph from a full graph containing only the root
 *                     recipe's transitive upstream and downstream.
 *
 * All public methods are static — the class has no state.
 */
struct ARCCRAFTEDITOR_API FArcRecipeGraphBuilder
{
	/**
	 * Build the complete graph from explicitly supplied recipes and items.
	 * Does NOT touch the AssetRegistry, making it suitable for unit tests.
	 *
	 * @param Recipes   All recipes to include. Entries with a null Recipe are skipped.
	 * @param Items     All item records available for tag-matching. Entries with a null Item are skipped.
	 */
	static FArcRecipeGraphData BuildFromRecords(
		TArrayView<const FArcRecipeGraphRecipe> Recipes,
		TArrayView<const FArcRecipeGraphItem> Items);

	/**
	 * Enumerate all indexed recipe and item records,
	 * then delegate to BuildFromRecords.
	 * Editor-only: requires a running UAssetManager.
	 */
	static FArcRecipeGraphData BuildFull();

	/**
	 * Produce an induced subgraph from Full containing only nodes reachable from
	 * RootRecipe via upstream (what it needs) or downstream (what needs it) BFS.
	 * A visited set prevents infinite traversal on recipe cycles.
	 *
	 * @param Full        Previously built full graph.
	 * @param RootRecipe  Asset id of the recipe node to focus on.
	 * @return Induced subgraph, or an empty graph if RootRecipe is not found.
	 */
	static FArcRecipeGraphData BuildFocused(
		const FArcRecipeGraphData& Full,
		FPrimaryAssetId RootRecipe);

	/**
	 * Populate a UArcRecipeGraph with UArcRecipeGraphNode objects derived from Data.
	 *
	 * Clears any existing nodes, creates one UArcRecipeGraphNode per
	 * FArcRecipeGraphNodeData, builds the pin link map, then connects all edges via
	 * MakeLinkTo.  Does NOT assign node positions (Step 3 / ArcRecipeGraphLayout
	 * handles layout).
	 *
	 * @param Graph  The graph to populate; existing nodes are removed first.
	 * @param Data   Pure-data representation produced by BuildFromRecords/BuildFull/BuildFocused.
	 */
	static void PopulateGraph(UArcRecipeGraph& Graph, const FArcRecipeGraphData& Data);
};
