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

/**
 * Full Sugiyama-framework layout pipeline for the recipe dependency graph.
 *
 * Pipeline (called via ApplySugiyama):
 *   RemoveCycles → RecomputeLayers → InsertDummies
 *     → ArcRecipeGraphLayoutLayered::MinimizeCrossings → GreedySwitch
 *     → ArcRecipeGraphLayoutLayered::AssignCoordinates  (Step 4b: replaced by Brandes-Köpf)
 *
 * All functions operate on a pure FArcRecipeLayoutModel — no UObject access.
 *
 * Dummy node contract (for Step 4b / Brandes-Köpf):
 *   - Dummy nodes are appended to Model.Nodes at indices [NumRealNodes, ...).
 *   - Each dummy belongs to exactly one intermediate layer; its layer index is
 *     stored in FArcRecipeLayoutNode::Layer.
 *   - Dummies are appended to their Layers arrays in edge-iteration order
 *     (deterministic; MinimizeCrossings will reorder within a layer).
 *   - An edge is an inner segment iff both endpoints have bDummy == true;
 *     no extra flag is needed (inner-segment detection for BK conflict marking).
 *   - Long-edge chains: for an original edge (U → W) spanning k layers, the
 *     chain is U → D1 → D2 → … → D(k-1) → W where D1…D(k-1) are consecutive
 *     new dummies.  The original edge is removed and replaced by k single-span
 *     edges.  bReversed is not propagated to dummy chain edges (all are
 *     correctly oriented after RemoveCycles + RecomputeLayers).
 */
namespace ArcRecipeGraphLayoutSugiyama
{

/**
 * Make the edge graph acyclic via DFS-based cycle removal.
 *
 * Performs a DFS from source nodes (in Model.Nodes array order).  When an edge
 * reaches a node that is currently on the DFS stack (back-edge), the edge's
 * From/To are swapped and bReversed is set to true.  After all reversals,
 * RebuildAdjacency is called to bring InNodes/OutNodes back in sync.
 *
 * @param Model  Layout model — Edges are modified in place; adjacency is rebuilt.
 */
ARCCRAFTEDITOR_API void RemoveCycles(FArcRecipeLayoutModel& Model);

/**
 * Recompute longest-path layers over the (now acyclic) edge set.
 *
 * After RemoveCycles the model's edges may differ from the original graph-pin
 * topology.  This pass performs a fresh longest-path layering so that
 * Layer(To) > Layer(From) holds for every edge.  Model.Layers arrays are
 * rebuilt and Model.Nodes[i].Layer is updated.
 *
 * @param Model  Layout model with acyclic edges (after RemoveCycles).
 */
ARCCRAFTEDITOR_API void RecomputeLayers(FArcRecipeLayoutModel& Model);

/**
 * Insert dummy nodes to ensure every edge spans exactly one layer.
 *
 * For each edge where Nodes[To].Layer - Nodes[From].Layer > 1, the edge is
 * replaced by a chain of single-layer-span edges through new dummy nodes.
 * Dummy nodes have bDummy = true and Size = (8, 8).
 *
 * Two-pass replacement: all new chains are collected before any removal,
 * then the old long edges are removed in a single bulk pass (never RemoveAt
 * inside the loop).
 *
 * New dummies are appended to Model.Nodes and to their respective Layers
 * arrays in edge-iteration order (deterministic).  RebuildAdjacency is called
 * at the end.
 *
 * @param Model  Layout model with layers recomputed (after RecomputeLayers).
 */
ARCCRAFTEDITOR_API void InsertDummies(FArcRecipeLayoutModel& Model);

/**
 * Greedy-switch post-pass to further reduce crossings after MinimizeCrossings.
 *
 * Within each layer, swaps adjacent node pairs when the swap reduces the
 * global crossing count (ArcRecipeGraphLayoutLayered::CountCrossings).
 * Repeats until no swapping pair is found.
 *
 * @param Model  Layout model with layers ordered by MinimizeCrossings.
 */
ARCCRAFTEDITOR_API void GreedySwitch(FArcRecipeLayoutModel& Model);

/**
 * Run the full Sugiyama pipeline on the model.
 *
 * Order:
 *   1. RemoveCycles
 *   2. RecomputeLayers
 *   3. InsertDummies
 *   4. ArcRecipeGraphLayoutLayered::MinimizeCrossings
 *   5. GreedySwitch
 *   6. ArcRecipeGraphLayoutLayered::AssignCoordinates
 *      // Step 4b: replaced by Brandes-Köpf
 *
 * @param Model  Layout model built by FArcRecipeGraphLayout::BuildModel.
 */
ARCCRAFTEDITOR_API void ApplySugiyama(FArcRecipeLayoutModel& Model);

} // namespace ArcRecipeGraphLayoutSugiyama
