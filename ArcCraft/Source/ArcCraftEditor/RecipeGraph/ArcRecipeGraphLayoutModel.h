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

/**
 * Pure layout model structs — no UObject types, fully unit-testable.
 *
 * Index contract
 * --------------
 * BuildModel populates real nodes first.  Model index i < NumRealNodes maps
 * 1:1 to the UArcRecipeGraphNode* at OutSourceNodes[i].  Dummy nodes (Sugiyama
 * virtual nodes) are only ever *appended* (index >= NumRealNodes).
 * WritePositions iterates [0, NumRealNodes) only.
 *
 * Inner segment (Brandes-Köpf): an edge is an inner segment iff both endpoints
 * have bDummy == true.  No extra flag needed.
 */

/** Represents one node (real or dummy) in the layout model. */
struct FArcRecipeLayoutNode
{
	/** Column layer assigned by longest-path layering. */
	int32 Layer = 0;

	/** Estimated or measured node size in Slate units. */
	FVector2D Size = FVector2D::ZeroVector;

	/** Output position, written by the layout algorithm. */
	FVector2D Pos = FVector2D::ZeroVector;

	/** True for Sugiyama virtual (dummy) nodes inserted on long edges. */
	bool bDummy = false;

	/** Upstream node indices (producers) — derived from Edges by RebuildAdjacency. */
	TArray<int32> InNodes;

	/** Downstream node indices (consumers) — derived from Edges by RebuildAdjacency. */
	TArray<int32> OutNodes;
};

/** Represents one directed edge (producer -> consumer) in the layout model. */
struct FArcRecipeLayoutEdge
{
	/** Producer node index. */
	int32 From = INDEX_NONE;

	/** Consumer node index. */
	int32 To = INDEX_NONE;

	/** Sugiyama cycle removal: From/To were swapped and this flag is set. */
	bool bReversed = false;
};

/** Complete layout model passed to all algorithm implementations. */
struct FArcRecipeLayoutModel
{
	TArray<FArcRecipeLayoutNode> Nodes;
	TArray<FArcRecipeLayoutEdge> Edges;

	/**
	 * Layers[L] holds the ordered node indices assigned to layer L.
	 * After BuildModel this is in insertion order; algorithms may reorder
	 * within a layer during crossing minimization.
	 */
	TArray<TArray<int32>> Layers;

	/**
	 * Number of real (non-dummy) nodes.  Indices [0, NumRealNodes) correspond
	 * exactly to the UArcRecipeGraphNode* array returned by BuildModel.
	 * Dummy nodes occupy indices [NumRealNodes, Nodes.Num()).
	 */
	int32 NumRealNodes = 0;
};
