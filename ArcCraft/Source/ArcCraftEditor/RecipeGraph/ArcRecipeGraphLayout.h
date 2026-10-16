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
#include "RecipeGraph/ArcRecipeGraphLayoutModel.h"

class UArcRecipeGraph;
class UArcRecipeGraphNode;

/**
 * Layout mode selector for the recipe dependency graph.
 *
 * Values map to the toolbar combo box labels in SArcRecipeGraphView.
 * Persisted as int32 in EditorPerProjectIni under [ArcCraftEditor] key
 * "RecipeGraphLayoutMode".  When adding a new enumerator, update the
 * clamp check in SArcRecipeGraphView::Construct that uses ForceDirected
 * as the upper bound.
 */
enum class EArcRecipeGraphLayoutMode : uint8
{
	/** Legacy fixed-grid: X = Layer * 320, Y = IndexInLayer * 120. */
	Compact,

	/** Barycenter crossing minimisation + size-aware Y packing (default). */
	MinCrossings,

	/** MinCrossings + median/priority Y refinement for chain straightening. */
	Straightened,

	/** Full Sugiyama: cycle removal, dummy nodes, greedy switch, Brandes-Köpf. */
	Sugiyama,

	/** Fruchterman-Reingold untangle seeded from MinCrossings result. */
	ForceDirected,
};

/**
 * Assigns NodePosX / NodePosY on every UArcRecipeGraphNode in the graph.
 *
 * Algorithm summary (Compact / legacy mode)
 * ------------------------------------------
 * 1. Collect all UArcRecipeGraphNode* from Graph.Nodes.
 * 2. Compute each node's layer via memoised DFS (longest-path / cycle-safe).
 * 3. NodePosX = Layer * 320.
 * 4. Within a layer nodes are stacked in Graph.Nodes array order:
 *      NodePosY = IndexInLayer * 120.
 *
 * Other modes build a FArcRecipeLayoutModel and operate on it before
 * writing back via WritePositions.  No Slate access in Compact mode.
 */
struct ARCCRAFTEDITOR_API FArcRecipeGraphLayout
{
	/**
	 * Apply the chosen layout mode to every UArcRecipeGraphNode in Graph.
	 *
	 * Until Steps 2-5 land, MinCrossings / Straightened / Sugiyama / ForceDirected
	 * all fall through to Compact.
	 */
	static void Apply(UArcRecipeGraph& Graph, EArcRecipeGraphLayoutMode Mode);

	/**
	 * Build the pure layout model from the graph.
	 *
	 * OutSourceNodes is populated in the same order as model indices
	 * [0, Model.NumRealNodes).
	 */
	static FArcRecipeLayoutModel BuildModel(
		UArcRecipeGraph& Graph,
		TArray<UArcRecipeGraphNode*>& OutSourceNodes);

	/**
	 * Rebuild InNodes / OutNodes on every node in Model from Model.Edges.
	 * Call after any operation that modifies Edges (dummy insertion, cycle removal).
	 */
	static void RebuildAdjacency(FArcRecipeLayoutModel& Model);

	/**
	 * Write Pos back to NodePosX / NodePosY for indices [0, NumRealNodes).
	 * Positions are rounded to integer (FMath::RoundToInt) to avoid near-miss
	 * overlaps from fractional BK compaction output.
	 */
	static void WritePositions(
		const FArcRecipeLayoutModel& Model,
		const TArray<UArcRecipeGraphNode*>& SourceNodes);
};
