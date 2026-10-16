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
 * Force-directed (Fruchterman-Reingold) untangle pass for the recipe graph.
 *
 * Caller contract
 * ---------------
 * The model must have already had MinimizeCrossings + AssignCoordinates applied
 * so that Pos (top-left) is set for all real nodes.  ApplyForceDirected runs the
 * FR simulation on node centres, then a pairwise overlap-separation post-pass,
 * and writes the final top-left positions back into Model.Nodes[i].Pos.
 *
 * Dummy nodes (bDummy == true) are skipped throughout — this path is seeded from
 * MinCrossings which never inserts dummies, but the guard is present defensively.
 */
namespace ArcRecipeGraphLayoutForce
{
	/**
	 * Apply a Fruchterman-Reingold simulation followed by a pairwise overlap-removal
	 * post-pass to the layout model.
	 *
	 * Input:  Model.Nodes[i].Pos is a top-left position (set by AssignCoordinates).
	 * Output: Model.Nodes[i].Pos updated in-place (still top-left).
	 *
	 * The simulation is deterministic — no RNG; seeded by the incoming positions.
	 */
	void ApplyForceDirected(FArcRecipeLayoutModel& Model);
} // namespace ArcRecipeGraphLayoutForce
