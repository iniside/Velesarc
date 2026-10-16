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

#include "RecipeGraph/ArcRecipeGraphLayoutLayered.h"

namespace ArcRecipeGraphLayoutLayered_Private
{

/** Gap between adjacent nodes within a layer (Slate units). */
static constexpr float IntraLayerGap = 24.0f;

/** Horizontal gap between adjacent layers (Slate units). */
static constexpr float InterLayerGap = 120.0f;

/**
 * Return the maximum node width in a given layer.
 * Returns 0 if the layer is empty.
 */
static float MaxNodeWidth(const FArcRecipeLayoutModel& Model, int32 LayerIndex)
{
	if (!Model.Layers.IsValidIndex(LayerIndex))
	{
		return 0.0f;
	}

	float MaxWidth = 0.0f;
	for (int32 NodeIdx : Model.Layers[LayerIndex])
	{
		if (Model.Nodes.IsValidIndex(NodeIdx))
		{
			const float NodeWidth = Model.Nodes[NodeIdx].Size.X;
			if (NodeWidth > MaxWidth)
			{
				MaxWidth = NodeWidth;
			}
		}
	}
	return MaxWidth;
}

/**
 * Compute the barycenter for a node in the given layer based on neighbours
 * in adjacent layers on the swept side.
 *
 * Downward sweep: neighbours are InNodes in lower layers (Layer < NodeLayer).
 * Upward sweep:   neighbours are OutNodes in higher layers (Layer > NodeLayer).
 *
 * Normalised ordinal = ordinal / max(1, layerSize - 1).
 * For a node with no qualifying neighbours returns its own current normalised ordinal.
 *
 * @param Model        Layout model.
 * @param NodeIdx      Node being evaluated.
 * @param OrdinalMap   Map from node index to current ordinal-in-layer.
 * @param bDownward    True = use InNodes (lower layers); false = use OutNodes (higher layers).
 * @return Barycenter value in [0, 1] or the node's own normalised ordinal if no neighbours.
 */
static float ComputeBarycenter(
	const FArcRecipeLayoutModel& Model,
	int32 NodeIdx,
	const TArray<int32>& OrdinalMap,
	bool bDownward)
{
	if (!Model.Nodes.IsValidIndex(NodeIdx))
	{
		return 0.0f;
	}

	const FArcRecipeLayoutNode& Node = Model.Nodes[NodeIdx];
	const int32 NodeLayer = Node.Layer;

	const TArray<int32>& Neighbours = bDownward ? Node.InNodes : Node.OutNodes;

	float Sum = 0.0f;
	int32 Count = 0;

	for (int32 NeighbourIdx : Neighbours)
	{
		if (!Model.Nodes.IsValidIndex(NeighbourIdx))
		{
			continue;
		}

		const int32 NeighbourLayer = Model.Nodes[NeighbourIdx].Layer;

		// Downward sweep: count neighbours in lower layers.
		// Upward sweep:   count neighbours in higher layers.
		const bool bQualifies = bDownward
			? (NeighbourLayer < NodeLayer)
			: (NeighbourLayer > NodeLayer);

		if (!bQualifies)
		{
			continue;
		}

		// Normalised ordinal of the neighbour within its layer.
		if (!OrdinalMap.IsValidIndex(NeighbourIdx))
		{
			continue;
		}

		const int32 NeighbourOrdinal = OrdinalMap[NeighbourIdx];
		const int32 NeighbourLayerSize = Model.Layers.IsValidIndex(NeighbourLayer)
			? Model.Layers[NeighbourLayer].Num()
			: 1;

		const float NormOrdinal = static_cast<float>(NeighbourOrdinal)
			/ static_cast<float>(FMath::Max(1, NeighbourLayerSize - 1));

		Sum += NormOrdinal;
		++Count;
	}

	if (Count == 0)
	{
		// No qualifying neighbours — return the node's own normalised ordinal.
		const int32 OwnOrdinal = OrdinalMap.IsValidIndex(NodeIdx) ? OrdinalMap[NodeIdx] : 0;
		const int32 OwnLayerSize = Model.Layers.IsValidIndex(NodeLayer)
			? Model.Layers[NodeLayer].Num()
			: 1;

		return static_cast<float>(OwnOrdinal)
			/ static_cast<float>(FMath::Max(1, OwnLayerSize - 1));
	}

	return Sum / static_cast<float>(Count);
}

/**
 * Build an ordinal map: OrdinalMap[NodeIdx] = position of that node within its layer.
 * Derived from the current Layers ordering.
 */
static TArray<int32> BuildOrdinalMap(const FArcRecipeLayoutModel& Model)
{
	TArray<int32> OrdinalMap;
	OrdinalMap.SetNum(Model.Nodes.Num());

	for (const TArray<int32>& Layer : Model.Layers)
	{
		for (int32 Ordinal = 0; Ordinal < Layer.Num(); ++Ordinal)
		{
			const int32 NodeIdx = Layer[Ordinal];
			if (OrdinalMap.IsValidIndex(NodeIdx))
			{
				OrdinalMap[NodeIdx] = Ordinal;
			}
		}
	}

	return OrdinalMap;
}

/**
 * Perform one sweep (either downward or upward) over all layers.
 * Reorders each layer by barycenter of its neighbours in the fixed direction.
 */
static void PerformSweep(FArcRecipeLayoutModel& Model, bool bDownward)
{
	TArray<int32> OrdinalMap = BuildOrdinalMap(Model);

	const int32 NumLayers = Model.Layers.Num();

	// Downward: process layers 1..N-1 (layer 0 has no lower neighbours, stays fixed).
	// Upward:   process layers N-2..0 (last layer has no higher neighbours, stays fixed).
	if (bDownward)
	{
		for (int32 L = 1; L < NumLayers; ++L)
		{
			TArray<int32>& Layer = Model.Layers[L];

			// Pair each node with its barycenter, then stable-sort by barycenter.
			TArray<TPair<float, int32>> BarycentersWithIndex;
			BarycentersWithIndex.Reserve(Layer.Num());

			for (int32 Ordinal = 0; Ordinal < Layer.Num(); ++Ordinal)
			{
				const int32 NodeIdx = Layer[Ordinal];
				const float Bary = ComputeBarycenter(Model, NodeIdx, OrdinalMap, /*bDownward=*/true);
				BarycentersWithIndex.Add(TPair<float, int32>(Bary, NodeIdx));
			}

			// Stable sort preserves relative order for equal barycenters.
			BarycentersWithIndex.StableSort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
			{
				return A.Key < B.Key;
			});

			for (int32 Ordinal = 0; Ordinal < Layer.Num(); ++Ordinal)
			{
				Layer[Ordinal] = BarycentersWithIndex[Ordinal].Value;
			}
		}
	}
	else
	{
		for (int32 L = NumLayers - 2; L >= 0; --L)
		{
			TArray<int32>& Layer = Model.Layers[L];

			TArray<TPair<float, int32>> BarycentersWithIndex;
			BarycentersWithIndex.Reserve(Layer.Num());

			for (int32 Ordinal = 0; Ordinal < Layer.Num(); ++Ordinal)
			{
				const int32 NodeIdx = Layer[Ordinal];
				const float Bary = ComputeBarycenter(Model, NodeIdx, OrdinalMap, /*bDownward=*/false);
				BarycentersWithIndex.Add(TPair<float, int32>(Bary, NodeIdx));
			}

			BarycentersWithIndex.StableSort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
			{
				return A.Key < B.Key;
			});

			for (int32 Ordinal = 0; Ordinal < Layer.Num(); ++Ordinal)
			{
				Layer[Ordinal] = BarycentersWithIndex[Ordinal].Value;
			}
		}
	}
}

} // namespace ArcRecipeGraphLayoutLayered_Private

// ----------------------------------------------------------------------------
// ArcRecipeGraphLayoutLayered — public API
// ----------------------------------------------------------------------------

namespace ArcRecipeGraphLayoutLayered
{

int32 CountCrossings(const FArcRecipeLayoutModel& Model)
{
	int32 TotalCrossings = 0;

	const int32 NumLayers = Model.Layers.Num();

	for (int32 L = 0; L + 1 < NumLayers; ++L)
	{
		const TArray<int32>& LayerA = Model.Layers[L];
		const TArray<int32>& LayerB = Model.Layers[L + 1];

		// Build ordinal maps for the two adjacent layers.
		TMap<int32, int32> OrdinalInA;
		OrdinalInA.Reserve(LayerA.Num());
		for (int32 Ordinal = 0; Ordinal < LayerA.Num(); ++Ordinal)
		{
			OrdinalInA.Add(LayerA[Ordinal], Ordinal);
		}

		TMap<int32, int32> OrdinalInB;
		OrdinalInB.Reserve(LayerB.Num());
		for (int32 Ordinal = 0; Ordinal < LayerB.Num(); ++Ordinal)
		{
			OrdinalInB.Add(LayerB[Ordinal], Ordinal);
		}

		// Collect edges between layer L and layer L+1.
		// An edge between layer L and layer L+1 can be direct (adjacent) or
		// long (spanning multiple layers) — we only count endpoints in these two layers.
		TArray<TPair<int32, int32>> EdgePairs;

		for (const FArcRecipeLayoutEdge& Edge : Model.Edges)
		{
			// Edge must have one endpoint in LayerA and one in LayerB.
			const int32* OrdA = OrdinalInA.Find(Edge.From);
			const int32* OrdB = OrdinalInB.Find(Edge.To);

			if (OrdA != nullptr && OrdB != nullptr)
			{
				EdgePairs.Add(TPair<int32, int32>(*OrdA, *OrdB));
				continue;
			}

			// Also handle reversed case (from higher layer to lower — Sugiyama cycle removal).
			const int32* OrdA2 = OrdinalInA.Find(Edge.To);
			const int32* OrdB2 = OrdinalInB.Find(Edge.From);

			if (OrdA2 != nullptr && OrdB2 != nullptr)
			{
				EdgePairs.Add(TPair<int32, int32>(*OrdA2, *OrdB2));
			}
		}

		// Naive O(E²) inversion count: edge (a1,b1) and (a2,b2) cross iff
		// (a1 < a2 && b1 > b2) || (a1 > a2 && b1 < b2).
		const int32 NumEdgePairs = EdgePairs.Num();
		for (int32 i = 0; i < NumEdgePairs; ++i)
		{
			for (int32 j = i + 1; j < NumEdgePairs; ++j)
			{
				const int32 A1 = EdgePairs[i].Key;
				const int32 B1 = EdgePairs[i].Value;
				const int32 A2 = EdgePairs[j].Key;
				const int32 B2 = EdgePairs[j].Value;

				if ((A1 < A2 && B1 > B2) || (A1 > A2 && B1 < B2))
				{
					++TotalCrossings;
				}
			}
		}
	}

	return TotalCrossings;
}

void MinimizeCrossings(FArcRecipeLayoutModel& Model, int32 MaxSweepPairs)
{
	if (Model.Layers.Num() < 2)
	{
		return;
	}

	// Snapshot the current best ordering.
	TArray<TArray<int32>> BestLayers = Model.Layers;
	int32 BestCrossings = CountCrossings(Model);

	int32 NonImprovingPairs = 0;

	for (int32 Pair = 0; Pair < MaxSweepPairs; ++Pair)
	{
		// Downward sweep.
		ArcRecipeGraphLayoutLayered_Private::PerformSweep(Model, /*bDownward=*/true);

		// Upward sweep.
		ArcRecipeGraphLayoutLayered_Private::PerformSweep(Model, /*bDownward=*/false);

		const int32 CurrentCrossings = CountCrossings(Model);

		if (CurrentCrossings < BestCrossings)
		{
			BestCrossings = CurrentCrossings;
			BestLayers = Model.Layers;
			NonImprovingPairs = 0;
		}
		else
		{
			++NonImprovingPairs;
			if (NonImprovingPairs >= 2)
			{
				// Two consecutive non-improving pairs — early out.
				break;
			}
		}
	}

	// Restore the best known ordering.
	Model.Layers = BestLayers;
}

void AssignCoordinates(FArcRecipeLayoutModel& Model)
{
	const int32 NumLayers = Model.Layers.Num();

	if (NumLayers == 0)
	{
		return;
	}

	// Build per-layer X positions: ColumnX[L] = ColumnX[L-1] + MaxWidth(L-1) + 120.
	TArray<float> ColumnX;
	ColumnX.SetNum(NumLayers);
	ColumnX[0] = 0.0f;

	for (int32 L = 1; L < NumLayers; ++L)
	{
		ColumnX[L] = ColumnX[L - 1]
			+ ArcRecipeGraphLayoutLayered_Private::MaxNodeWidth(Model, L - 1)
			+ ArcRecipeGraphLayoutLayered_Private::InterLayerGap;
	}

	// Assign Y positions within each layer: stack top-to-bottom with 24px gap,
	// then offset the whole layer by -TotalLayerHeight/2 to centre on Y=0.
	for (int32 L = 0; L < NumLayers; ++L)
	{
		const TArray<int32>& Layer = Model.Layers[L];
		const int32 LayerSize = Layer.Num();

		if (LayerSize == 0)
		{
			continue;
		}

		// Compute total layer height.
		float TotalHeight = 0.0f;
		for (int32 i = 0; i < LayerSize; ++i)
		{
			const int32 NodeIdx = Layer[i];
			if (Model.Nodes.IsValidIndex(NodeIdx))
			{
				TotalHeight += Model.Nodes[NodeIdx].Size.Y;
			}
			if (i + 1 < LayerSize)
			{
				TotalHeight += ArcRecipeGraphLayoutLayered_Private::IntraLayerGap;
			}
		}

		const float LayerCentreOffset = -TotalHeight * 0.5f;

		// Assign positions.
		float RunningY = LayerCentreOffset;
		for (int32 i = 0; i < LayerSize; ++i)
		{
			const int32 NodeIdx = Layer[i];
			if (!Model.Nodes.IsValidIndex(NodeIdx))
			{
				continue;
			}

			FArcRecipeLayoutNode& Node = Model.Nodes[NodeIdx];
			Node.Pos.X = ColumnX[L];
			Node.Pos.Y = RunningY;

			RunningY += Node.Size.Y + ArcRecipeGraphLayoutLayered_Private::IntraLayerGap;
		}
	}
}

void StraightenY(FArcRecipeLayoutModel& Model, int32 NumPasses)
{
	using namespace ArcRecipeGraphLayoutLayered_Private;

	const int32 NumNodes = Model.Nodes.Num();
	if (NumNodes == 0)
	{
		return;
	}

	// Large priority bonus for dummy nodes so they move first.
	static constexpr int32 DummyPriorityBonus = 1000000;

	// Minimum gap between node edges within the same layer.
	static constexpr float MinEdgeGap = 24.0f;

	// Per-pass: alternate downstream (OutNodes) then upstream (InNodes).
	for (int32 Pass = 0; Pass < NumPasses; ++Pass)
	{
		const bool bDownstream = (Pass % 2 == 0);

		const int32 NumLayers = Model.Layers.Num();
		for (int32 L = 0; L < NumLayers; ++L)
		{
			const TArray<int32>& Layer = Model.Layers[L];
			const int32 LayerSize = Layer.Num();
			if (LayerSize == 0)
			{
				continue;
			}

			// Build priority for each node in this layer.
			// Priority = total degree + dummy bonus.
			TArray<TPair<int32, int32>> PriorityAndIndex;
			PriorityAndIndex.Reserve(LayerSize);

			for (int32 Ordinal = 0; Ordinal < LayerSize; ++Ordinal)
			{
				const int32 NodeIdx = Layer[Ordinal];
				if (!Model.Nodes.IsValidIndex(NodeIdx))
				{
					continue;
				}
				const FArcRecipeLayoutNode& N = Model.Nodes[NodeIdx];
				int32 Priority = N.InNodes.Num() + N.OutNodes.Num();
				if (N.bDummy)
				{
					Priority += DummyPriorityBonus;
				}
				PriorityAndIndex.Add(TPair<int32, int32>(Priority, Ordinal));
			}

			// Sort descending by priority.
			PriorityAndIndex.Sort([](const TPair<int32, int32>& A, const TPair<int32, int32>& B)
			{
				return A.Key > B.Key;
			});

			// Track which ordinals have already been placed (set of processed ordinals).
			// We'll maintain per-ordinal "placed" state as an array of bools indexed by ordinal.
			TArray<bool> bPlaced;
			bPlaced.SetNumZeroed(LayerSize);

			// Process nodes in descending priority order.
			for (const TPair<int32, int32>& PriorityEntry : PriorityAndIndex)
			{
				const int32 Ordinal = PriorityEntry.Value;
				const int32 NodeIdx = Layer[Ordinal];
				if (!Model.Nodes.IsValidIndex(NodeIdx))
				{
					bPlaced[Ordinal] = true;
					continue;
				}

				FArcRecipeLayoutNode& Node = Model.Nodes[NodeIdx];

				// Gather connected neighbors in the sweep direction.
				const TArray<int32>& Neighbors = bDownstream ? Node.OutNodes : Node.InNodes;

				// Collect neighbor center-Y values.
				TArray<float> NeighborCenterYs;
				NeighborCenterYs.Reserve(Neighbors.Num());
				for (int32 NeighborIdx : Neighbors)
				{
					if (Model.Nodes.IsValidIndex(NeighborIdx))
					{
						const FArcRecipeLayoutNode& Neighbor = Model.Nodes[NeighborIdx];
						NeighborCenterYs.Add(Neighbor.Pos.Y + Neighbor.Size.Y * 0.5f);
					}
				}

				// Skip if no qualifying neighbors.
				if (NeighborCenterYs.Num() == 0)
				{
					bPlaced[Ordinal] = true;
					continue;
				}

				// Compute median of neighbor center-Y values.
				NeighborCenterYs.Sort([](float A, float B) { return A < B; });
				const int32 NumNeighbors = NeighborCenterYs.Num();
				float DesiredCenterY;
				if (NumNeighbors % 2 == 1)
				{
					DesiredCenterY = NeighborCenterYs[NumNeighbors / 2];
				}
				else
				{
					DesiredCenterY = (NeighborCenterYs[NumNeighbors / 2 - 1] + NeighborCenterYs[NumNeighbors / 2]) * 0.5f;
				}

				// Clamp DesiredCenterY to preserve relative Y order within the layer
				// and maintain MinEdgeGap from already-placed higher-priority neighbors.
				//
				// Higher-priority nodes have been placed already (bPlaced[Ordinal] for smaller ordinals).
				// Within the layer the Y ordering must not be violated:
				//   - The node at Ordinal must stay above (lower Y) nodes at Ordinal+1..N-1 that are already placed.
				//   - The node at Ordinal must stay below (higher Y) nodes at 0..Ordinal-1 that are already placed.
				//
				// Clamp range: [MinAllowedTop, MaxAllowedTop]
				//   MinAllowedTop: just below the closest placed node above this one in ordinal
				//   MaxAllowedTop: just above the closest placed node below this one in ordinal

				float ClampMin = -FLT_MAX;  // minimum allowed Pos.Y (top edge)
				float ClampMax =  FLT_MAX;  // maximum allowed Pos.Y (top edge)

				// Find the nearest already-placed neighbor above in ordinal order (lower ordinal = higher up).
				for (int32 AboveOrdinal = Ordinal - 1; AboveOrdinal >= 0; --AboveOrdinal)
				{
					if (!bPlaced[AboveOrdinal])
					{
						continue;
					}
					const int32 AboveNodeIdx = Layer[AboveOrdinal];
					if (!Model.Nodes.IsValidIndex(AboveNodeIdx))
					{
						continue;
					}
					const FArcRecipeLayoutNode& AboveNode = Model.Nodes[AboveNodeIdx];
					// This node's top must be at least AboveNode.Pos.Y + AboveNode.Size.Y + MinEdgeGap.
					const float MinTop = AboveNode.Pos.Y + AboveNode.Size.Y + MinEdgeGap;
					if (MinTop > ClampMin)
					{
						ClampMin = MinTop;
					}
					break;  // Only care about the nearest.
				}

				// Find the nearest already-placed neighbor below in ordinal order.
				for (int32 BelowOrdinal = Ordinal + 1; BelowOrdinal < LayerSize; ++BelowOrdinal)
				{
					if (!bPlaced[BelowOrdinal])
					{
						continue;
					}
					const int32 BelowNodeIdx = Layer[BelowOrdinal];
					if (!Model.Nodes.IsValidIndex(BelowNodeIdx))
					{
						continue;
					}
					const FArcRecipeLayoutNode& BelowNode = Model.Nodes[BelowNodeIdx];
					// This node's bottom must be at most BelowNode.Pos.Y - MinEdgeGap.
					// So top must be at most BelowNode.Pos.Y - MinEdgeGap - Node.Size.Y.
					const float MaxTop = BelowNode.Pos.Y - MinEdgeGap - Node.Size.Y;
					if (MaxTop < ClampMax)
					{
						ClampMax = MaxTop;
					}
					break;  // Only care about the nearest.
				}

				// Convert desired center-Y to top-Y.
				const float DesiredTop = DesiredCenterY - Node.Size.Y * 0.5f;

				// Apply clamped position.
				if (ClampMin <= ClampMax)
				{
					Node.Pos.Y = FMath::Clamp(DesiredTop, ClampMin, ClampMax);
				}
				// If the clamp range is inverted (too-tight layer), leave the node where it is.

				bPlaced[Ordinal] = true;
			}
		}
	}
}

} // namespace ArcRecipeGraphLayoutLayered
