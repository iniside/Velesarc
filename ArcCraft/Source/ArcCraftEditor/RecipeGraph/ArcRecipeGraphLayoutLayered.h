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
 * Layered layout algorithms for the recipe dependency graph.
 *
 * All functions operate on a pure FArcRecipeLayoutModel — no UObject access.
 * The dispatch in FArcRecipeGraphLayout::Apply orchestrates the pipeline.
 *
 * MinCrossings pipeline:
 *   MinimizeCrossings → AssignCoordinates → (caller invokes WritePositions)
 *
 * Straightened pipeline (Step 3):
 *   MinimizeCrossings → AssignCoordinates → StraightenY → (caller invokes WritePositions)
 */
namespace ArcRecipeGraphLayoutLayered
{

/**
 * Count the total number of edge crossings in the model.
 *
 * For each adjacent layer pair (L, L+1), collects edges between those layers
 * as ordinal pairs (posInL, posInL+1) and counts inversions with a naive O(E²)
 * double loop.  Adequate at this graph scale; upgrade to merge-sort inversion
 * counter only if profiling demands it.
 *
 * @param Model  Layout model with Layers arrays fully populated.
 * @return       Total crossing count across all adjacent layer pairs.
 */
ARCCRAFTEDITOR_API int32 CountCrossings(const FArcRecipeLayoutModel& Model);

/**
 * Reorder nodes within each layer to reduce edge crossings.
 *
 * Alternates downward and upward barycenter sweeps:
 *   Downward: reorder layer L by mean normalised ordinal of upstream neighbours
 *             (InNodes in any lower layer).
 *   Upward:   reorder layer L by mean normalised ordinal of downstream neighbours
 *             (OutNodes in any higher layer).
 *
 * Normalised ordinal = ordinal / max(1, layerSize - 1) so ordinals from
 * layers of different sizes are comparable (handles edges spanning > 1 layer
 * without Sugiyama dummy nodes).
 *
 * Nodes with no neighbours on the swept side keep their current normalised
 * ordinal as barycenter.  Within equal barycenters the current relative order
 * is preserved (stable sort).
 *
 * After each down+up pair CountCrossings is called; the best Layers ordering
 * is remembered and restored at exit.  Stops early when two consecutive pairs
 * produce no improvement.
 *
 * @param Model          Layout model — Layers arrays are reordered in place.
 * @param MaxSweepPairs  Maximum number of down+up sweep pairs before giving up.
 */
ARCCRAFTEDITOR_API void MinimizeCrossings(FArcRecipeLayoutModel& Model, int32 MaxSweepPairs = 10);

/**
 * Assign X/Y coordinates to every node in the model (size-aware packing).
 *
 * X placement (left to right):
 *   ColumnX[0] = 0
 *   ColumnX[L] = ColumnX[L-1] + MaxNodeWidth(L-1) + 120
 *
 * Y placement (within a layer):
 *   Nodes are stacked top-to-bottom with a 24-pixel gap between them.
 *   The entire layer is then offset by -(TotalLayerHeight / 2) so that
 *   every layer is vertically centred on Y = 0.
 *
 * Writes FArcRecipeLayoutNode::Pos for every node (real and dummy).
 * WritePositions subsequently copies Pos back to UEdGraphNode::NodePosX/Y
 * for real nodes only.
 *
 * @param Model  Layout model with Layers arrays ordered by MinimizeCrossings.
 */
ARCCRAFTEDITOR_API void AssignCoordinates(FArcRecipeLayoutModel& Model);

/**
 * Refine Y coordinates to straighten single-in / single-out chains (Step 3).
 *
 * Declared here for completeness; implemented in Step 3 of the plan.
 * Runs after AssignCoordinates and operates on Pos.Y only — the ordinal
 * Layers arrays are no longer consulted at this stage.
 *
 * @param Model     Layout model with coordinates already assigned.
 * @param NumPasses Number of alternating refinement passes.
 */
ARCCRAFTEDITOR_API void StraightenY(FArcRecipeLayoutModel& Model, int32 NumPasses = 8);

} // namespace ArcRecipeGraphLayoutLayered
