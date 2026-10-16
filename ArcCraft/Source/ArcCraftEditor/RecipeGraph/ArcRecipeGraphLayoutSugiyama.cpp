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

#include "RecipeGraph/ArcRecipeGraphLayoutSugiyama.h"
#include "RecipeGraph/ArcRecipeGraphLayout.h"
#include "RecipeGraph/ArcRecipeGraphLayoutLayered.h"

#include "Algo/Reverse.h"

namespace ArcRecipeGraphLayoutSugiyama_Private
{

/**
 * DFS helper for RemoveCycles.
 *
 * Visits node NodeIdx, marking it Grey (on DFS stack) then Black (done).
 * When an edge leads to a Grey node (back-edge), the edge is reversed in place
 * and bReversed is set.
 *
 * @param NodeIdx    Current node.
 * @param Model      Layout model (Edges modified in place).
 * @param NodeColor  0=White(unvisited), 1=Grey(on stack), 2=Black(done).
 */
static void DFSVisit(
	int32 NodeIdx,
	FArcRecipeLayoutModel& Model,
	TArray<uint8>& NodeColor)
{
	NodeColor[NodeIdx] = 1; // Grey — on DFS stack.

	const int32 NumEdges = Model.Edges.Num();
	for (int32 EdgeIdx = 0; EdgeIdx < NumEdges; ++EdgeIdx)
	{
		FArcRecipeLayoutEdge& Edge = Model.Edges[EdgeIdx];

		if (Edge.From != NodeIdx)
		{
			continue;
		}

		const int32 Target = Edge.To;
		if (!Model.Nodes.IsValidIndex(Target))
		{
			continue;
		}

		if (NodeColor[Target] == 1)
		{
			// Back-edge — reverse it to break the cycle.
			const int32 OldFrom = Edge.From;
			const int32 OldTo   = Edge.To;
			Edge.From      = OldTo;
			Edge.To        = OldFrom;
			Edge.bReversed = true;
			// This edge no longer originates from NodeIdx; don't recurse on it.
			continue;
		}

		if (NodeColor[Target] == 0)
		{
			DFSVisit(Target, Model, NodeColor);
		}
	}

	NodeColor[NodeIdx] = 2; // Black — done.
}

/**
 * Longest-path layer computation via Kahn's topological sort (iterative,
 * uses the in-degree of the post-RemoveCycles acyclic edge set).
 *
 * For each node, Layer = max(Layer(predecessor) + 1) over all incoming edges.
 * Returns a TArray<int32> of size Model.Nodes.Num() with the assigned layer.
 */
static TArray<int32> ComputeLongestPathLayers(const FArcRecipeLayoutModel& Model)
{
	const int32 NumNodes = Model.Nodes.Num();
	TArray<int32> InDegree;
	InDegree.SetNumZeroed(NumNodes);

	// Build in-degree from the current (acyclic) edges.
	for (const FArcRecipeLayoutEdge& Edge : Model.Edges)
	{
		if (Model.Nodes.IsValidIndex(Edge.From) && Model.Nodes.IsValidIndex(Edge.To))
		{
			++InDegree[Edge.To];
		}
	}

	// Layer assignment: layer[i] is the longest path from any source to node i.
	TArray<int32> Layer;
	Layer.SetNumZeroed(NumNodes);

	// Process nodes in topological order (Kahn's algorithm).
	TArray<int32> Queue;
	Queue.Reserve(NumNodes);
	for (int32 i = 0; i < NumNodes; ++i)
	{
		if (InDegree[i] == 0)
		{
			Queue.Add(i);
		}
	}

	int32 QueueHead = 0;
	while (QueueHead < Queue.Num())
	{
		const int32 NodeIdx = Queue[QueueHead];
		++QueueHead;

		// Propagate layer to successors.
		for (const FArcRecipeLayoutEdge& Edge : Model.Edges)
		{
			if (Edge.From != NodeIdx)
			{
				continue;
			}
			const int32 Successor = Edge.To;
			if (!Model.Nodes.IsValidIndex(Successor))
			{
				continue;
			}

			const int32 CandidateLayer = Layer[NodeIdx] + 1;
			if (CandidateLayer > Layer[Successor])
			{
				Layer[Successor] = CandidateLayer;
			}

			--InDegree[Successor];
			if (InDegree[Successor] == 0)
			{
				Queue.Add(Successor);
			}
		}
	}

	return Layer;
}

// ----------------------------------------------------------------------------
// Brandes-Köpf coordinate assignment (Step 4b)
//
// Transposition note: the literature lays layers top-down and computes X; our
// layers are left-right COLUMNS, so every "horizontal/x" below is Y, "left/
// right within a layer" is up/down within a column, and "upper layer" is the
// previous column. Structure follows dagre position/bk.js and the published
// erratum (arXiv:2008.01252), not the 2001 paper pseudocode.
//
// All Y values are node CENTER coordinates until the final write, where the
// top-left convention (matching AssignCoordinates) is restored.
// ----------------------------------------------------------------------------

/** Must match ArcRecipeGraphLayoutLayered_Private::IntraLayerGap / InterLayerGap. */
static constexpr float BKIntraLayerGap = 24.0f;
static constexpr float BKInterLayerGap = 120.0f;

/** Order-independent key for a node pair (conflict set). */
static uint64 PackNodePair(int32 A, int32 B)
{
	const uint64 Lo = static_cast<uint64>(static_cast<uint32>(FMath::Min(A, B)));
	const uint64 Hi = static_cast<uint64>(static_cast<uint32>(FMath::Max(A, B)));
	return (Hi << 32) | Lo;
}

/** Ordinals[NodeIdx] = position of the node within its Layers array. */
static TArray<int32> BuildOrdinals(const FArcRecipeLayoutModel& Model)
{
	TArray<int32> Ordinals;
	Ordinals.SetNumZeroed(Model.Nodes.Num());
	for (const TArray<int32>& Layer : Model.Layers)
	{
		for (int32 I = 0; I < Layer.Num(); ++I)
		{
			if (Ordinals.IsValidIndex(Layer[I]))
			{
				Ordinals[Layer[I]] = I;
			}
		}
	}
	return Ordinals;
}

/**
 * Mark type-1 conflicts: non-inner segments crossing inner (dummy-dummy)
 * segments between each pair of adjacent columns. Inner segments win; the
 * crossing non-inner segment is barred from alignment in all four passes.
 */
static TSet<uint64> MarkType1Conflicts(const FArcRecipeLayoutModel& Model, const TArray<int32>& Ordinals)
{
	TSet<uint64> Conflicts;

	for (int32 L = 0; L + 1 < Model.Layers.Num(); ++L)
	{
		const TArray<int32>& PrevLayer = Model.Layers[L];
		const TArray<int32>& CurLayer  = Model.Layers[L + 1];
		if (PrevLayer.Num() == 0 || CurLayer.Num() == 0)
		{
			continue;
		}

		int32 K0 = 0;
		int32 ScanStart = 0;

		for (int32 L1 = 0; L1 < CurLayer.Num(); ++L1)
		{
			const int32 V = CurLayer[L1];

			// Upper endpoint of an inner segment ending at V, if any.
			int32 InnerUpper = INDEX_NONE;
			if (Model.Nodes.IsValidIndex(V) && Model.Nodes[V].bDummy)
			{
				for (int32 U : Model.Nodes[V].InNodes)
				{
					if (Model.Nodes.IsValidIndex(U) && Model.Nodes[U].bDummy && Model.Nodes[U].Layer == L)
					{
						InnerUpper = U;
						break;
					}
				}
			}

			const bool bLast = (L1 == CurLayer.Num() - 1);
			if (!bLast && InnerUpper == INDEX_NONE)
			{
				continue;
			}

			const int32 K1 = (InnerUpper != INDEX_NONE) ? Ordinals[InnerUpper] : (PrevLayer.Num() - 1);

			for (int32 Sp = ScanStart; Sp <= L1; ++Sp)
			{
				const int32 W = CurLayer[Sp];
				if (!Model.Nodes.IsValidIndex(W))
				{
					continue;
				}
				for (int32 U : Model.Nodes[W].InNodes)
				{
					if (!Model.Nodes.IsValidIndex(U) || Model.Nodes[U].Layer != L)
					{
						continue;
					}
					const int32 PosU = Ordinals[U];
					if (PosU < K0 || PosU > K1)
					{
						Conflicts.Add(PackNodePair(U, W));
					}
				}
			}

			ScanStart = L1 + 1;
			K0 = K1;
		}
	}

	return Conflicts;
}

/** Required center-to-center separation between two vertically adjacent nodes. */
static float BKSeparation(const FArcRecipeLayoutModel& Model, int32 Upper, int32 Lower)
{
	return (Model.Nodes[Upper].Size.Y + Model.Nodes[Lower].Size.Y) * 0.5f + BKIntraLayerGap;
}

/** Shared state for the horizontal-compaction phase of one BK pass. */
struct FBKCompactionState
{
	const FArcRecipeLayoutModel* Model = nullptr;
	const TArray<TArray<int32>>* TColumns = nullptr; // transformed columns
	const TArray<int32>* TPos = nullptr;             // transformed in-column ordinal
	const TArray<int32>* ColumnOf = nullptr;
	const TArray<int32>* Root = nullptr;
	const TArray<int32>* Align = nullptr;
	TArray<int32> Sink;
	TArray<float> Shift;
	TArray<float> BlockY;   // per root-block center Y (transformed space)
	TArray<bool>  bDefined;
};

/** place_block from the erratum: longest-path placement of block classes with sink/shift bookkeeping. */
static void BKPlaceBlock(FBKCompactionState& S, int32 V)
{
	if (S.bDefined[V])
	{
		return;
	}
	S.bDefined[V] = true;
	S.BlockY[V] = 0.0f;

	int32 W = V;
	do
	{
		const int32 Col  = (*S.ColumnOf)[W];
		const int32 PosW = (*S.TPos)[W];

		if (PosW > 0)
		{
			const int32 Pred = (*S.TColumns)[Col][PosW - 1];
			const int32 U = (*S.Root)[Pred];
			BKPlaceBlock(S, U);

			if (S.Sink[V] == V)
			{
				S.Sink[V] = S.Sink[U];
			}

			const float Sep = BKSeparation(*S.Model, Pred, W);
			if (S.Sink[V] != S.Sink[U])
			{
				S.Shift[S.Sink[U]] = FMath::Min(S.Shift[S.Sink[U]], S.BlockY[V] - S.BlockY[U] - Sep);
			}
			else
			{
				S.BlockY[V] = FMath::Max(S.BlockY[V], S.BlockY[U] + Sep);
			}
		}

		W = (*S.Align)[W];
	}
	while (W != V);
}

/**
 * One of the four BK passes. Returns per-node center Y in ORIGINAL (unflipped) space.
 *
 * @param bUseInNeighbors  true: align against the previous column (uses InNodes,
 *                         columns processed left-to-right); false: next column
 *                         (OutNodes, right-to-left).
 * @param bReverseInColumn true: down-biased pass — in-column order is flipped and
 *                         the resulting coordinates negated back.
 */
static TArray<float> RunBKPass(
	const FArcRecipeLayoutModel& Model,
	const TSet<uint64>& Conflicts,
	bool bUseInNeighbors,
	bool bReverseInColumn)
{
	const int32 NumNodes = Model.Nodes.Num();
	const int32 NumColumns = Model.Layers.Num();

	// Transformed columns (flip in-column order for down-biased passes).
	TArray<TArray<int32>> TColumns = Model.Layers;
	if (bReverseInColumn)
	{
		for (TArray<int32>& Column : TColumns)
		{
			Algo::Reverse(Column);
		}
	}

	TArray<int32> TPos;
	TPos.SetNumZeroed(NumNodes);
	TArray<int32> ColumnOf;
	ColumnOf.SetNumZeroed(NumNodes);
	for (int32 Col = 0; Col < NumColumns; ++Col)
	{
		for (int32 I = 0; I < TColumns[Col].Num(); ++I)
		{
			const int32 NodeIdx = TColumns[Col][I];
			if (TPos.IsValidIndex(NodeIdx))
			{
				TPos[NodeIdx] = I;
				ColumnOf[NodeIdx] = Col;
			}
		}
	}

	// --- Vertical alignment: build root/align block structure. ---
	TArray<int32> Root;
	Root.SetNum(NumNodes);
	TArray<int32> Align;
	Align.SetNum(NumNodes);
	for (int32 I = 0; I < NumNodes; ++I)
	{
		Root[I] = I;
		Align[I] = I;
	}

	const int32 ColStart = bUseInNeighbors ? 1 : NumColumns - 2;
	const int32 ColEnd   = bUseInNeighbors ? NumColumns : -1;
	const int32 ColStep  = bUseInNeighbors ? 1 : -1;

	for (int32 Col = ColStart; Col != ColEnd && NumColumns > 1; Col += ColStep)
	{
		if (!TColumns.IsValidIndex(Col))
		{
			break;
		}

		const int32 FixedCol = Col - ColStep;
		int32 R = -1;

		const TArray<int32>& Column = TColumns[Col];
		for (int32 I = 0; I < Column.Num(); ++I)
		{
			const int32 V = Column[I];

			// Neighbors in the fixed adjacent column, sorted by transformed ordinal.
			TArray<int32> Neighbors;
			const TArray<int32>& Candidates = bUseInNeighbors ? Model.Nodes[V].InNodes : Model.Nodes[V].OutNodes;
			for (int32 U : Candidates)
			{
				if (ColumnOf.IsValidIndex(U) && ColumnOf[U] == FixedCol)
				{
					Neighbors.Add(U);
				}
			}
			if (Neighbors.Num() == 0)
			{
				continue;
			}
			Neighbors.Sort([&TPos](int32 A, int32 B) { return TPos[A] < TPos[B]; });

			const int32 D = Neighbors.Num();
			const int32 MedianLo = (D - 1) / 2;
			const int32 MedianHi = D / 2;

			for (int32 M = MedianLo; M <= MedianHi; ++M)
			{
				if (Align[V] != V)
				{
					break;
				}
				const int32 U = Neighbors[M];
				if (Conflicts.Contains(PackNodePair(U, V)))
				{
					continue;
				}
				if (TPos[U] <= R)
				{
					continue;
				}
				Align[U] = V;
				Root[V] = Root[U];
				Align[V] = Root[V];
				R = TPos[U];
			}
		}
	}

	// --- Horizontal compaction (computes Y here, per the transposition). ---
	FBKCompactionState State;
	State.Model = &Model;
	State.TColumns = &TColumns;
	State.TPos = &TPos;
	State.ColumnOf = &ColumnOf;
	State.Root = &Root;
	State.Align = &Align;
	State.Sink.SetNum(NumNodes);
	for (int32 I = 0; I < NumNodes; ++I)
	{
		State.Sink[I] = I;
	}
	State.Shift.Init(FLT_MAX, NumNodes);
	State.BlockY.SetNumZeroed(NumNodes);
	State.bDefined.Init(false, NumNodes);

	for (int32 V = 0; V < NumNodes; ++V)
	{
		if (Root[V] == V)
		{
			BKPlaceBlock(State, V);
		}
	}

	// Absolute coordinates: block Y plus the shift of the block's sink class.
	TArray<float> CenterY;
	CenterY.SetNumZeroed(NumNodes);
	for (int32 V = 0; V < NumNodes; ++V)
	{
		float YV = State.BlockY[Root[V]];
		const float SinkShift = State.Shift[State.Sink[Root[V]]];
		if (SinkShift < FLT_MAX)
		{
			YV += SinkShift;
		}
		// Un-flip down-biased passes back to original space.
		CenterY[V] = bReverseInColumn ? -YV : YV;
	}

	return CenterY;
}

/**
 * Full Brandes-Köpf: 4 alignment passes, balanced merge, column X placement.
 * Writes Model.Nodes[].Pos (top-left convention, matching AssignCoordinates).
 */
static void BrandesKoepfAssign(FArcRecipeLayoutModel& Model)
{
	const int32 NumNodes = Model.Nodes.Num();
	const int32 NumColumns = Model.Layers.Num();
	if (NumNodes == 0 || NumColumns == 0)
	{
		return;
	}

	const TArray<int32> Ordinals = BuildOrdinals(Model);
	const TSet<uint64> Conflicts = MarkType1Conflicts(Model, Ordinals);

	// 4 passes: {prev column, next column} x {up bias, down bias}.
	TArray<TArray<float>> PassY;
	PassY.Reserve(4);
	TArray<bool> PassIsDownBiased;
	PassIsDownBiased.Reserve(4);
	for (int32 PassIdx = 0; PassIdx < 4; ++PassIdx)
	{
		const bool bUseIn = (PassIdx < 2);
		const bool bReverse = (PassIdx % 2 == 1);
		PassY.Add(RunBKPass(Model, Conflicts, bUseIn, bReverse));
		PassIsDownBiased.Add(bReverse);
	}

	// Balance: align all 4 assignments to the one with the smallest extent —
	// up-biased passes align on their minimum, down-biased on their maximum.
	float PassMin[4];
	float PassMax[4];
	for (int32 P = 0; P < 4; ++P)
	{
		PassMin[P] = FLT_MAX;
		PassMax[P] = -FLT_MAX;
		for (int32 V = 0; V < NumNodes; ++V)
		{
			PassMin[P] = FMath::Min(PassMin[P], PassY[P][V]);
			PassMax[P] = FMath::Max(PassMax[P], PassY[P][V]);
		}
	}

	int32 SmallestPass = 0;
	for (int32 P = 1; P < 4; ++P)
	{
		if ((PassMax[P] - PassMin[P]) < (PassMax[SmallestPass] - PassMin[SmallestPass]))
		{
			SmallestPass = P;
		}
	}

	for (int32 P = 0; P < 4; ++P)
	{
		const float Offset = PassIsDownBiased[P]
			? (PassMax[SmallestPass] - PassMax[P])
			: (PassMin[SmallestPass] - PassMin[P]);
		if (Offset != 0.0f)
		{
			for (int32 V = 0; V < NumNodes; ++V)
			{
				PassY[P][V] += Offset;
			}
		}
	}

	// Column X placement, identical scheme to AssignCoordinates.
	TArray<float> ColumnX;
	ColumnX.SetNumZeroed(NumColumns);
	for (int32 L = 1; L < NumColumns; ++L)
	{
		float MaxWidth = 0.0f;
		for (int32 NodeIdx : Model.Layers[L - 1])
		{
			if (Model.Nodes.IsValidIndex(NodeIdx))
			{
				MaxWidth = FMath::Max(MaxWidth, Model.Nodes[NodeIdx].Size.X);
			}
		}
		ColumnX[L] = ColumnX[L - 1] + MaxWidth + BKInterLayerGap;
	}

	// Final per-node Y = average of the two median candidates; write top-left Pos.
	for (int32 V = 0; V < NumNodes; ++V)
	{
		float Vals[4] = { PassY[0][V], PassY[1][V], PassY[2][V], PassY[3][V] };
		// Tiny fixed-size sort.
		for (int32 A = 0; A < 3; ++A)
		{
			for (int32 B = A + 1; B < 4; ++B)
			{
				if (Vals[B] < Vals[A])
				{
					const float Tmp = Vals[A];
					Vals[A] = Vals[B];
					Vals[B] = Tmp;
				}
			}
		}
		const float CenterY = (Vals[1] + Vals[2]) * 0.5f;

		FArcRecipeLayoutNode& Node = Model.Nodes[V];
		Node.Pos.X = ColumnX.IsValidIndex(Node.Layer) ? ColumnX[Node.Layer] : 0.0f;
		Node.Pos.Y = CenterY - Node.Size.Y * 0.5f;
	}
}

} // namespace ArcRecipeGraphLayoutSugiyama_Private

// ----------------------------------------------------------------------------
// ArcRecipeGraphLayoutSugiyama — public API
// ----------------------------------------------------------------------------

namespace ArcRecipeGraphLayoutSugiyama
{

void RemoveCycles(FArcRecipeLayoutModel& Model)
{
	const int32 NumNodes = Model.Nodes.Num();
	if (NumNodes == 0)
	{
		return;
	}

	// 0=White(unvisited), 1=Grey(on stack), 2=Black(done).
	TArray<uint8> NodeColor;
	NodeColor.SetNumZeroed(NumNodes);

	// Visit all nodes in array order so DFS is deterministic.
	for (int32 NodeIdx = 0; NodeIdx < NumNodes; ++NodeIdx)
	{
		if (NodeColor[NodeIdx] == 0)
		{
			ArcRecipeGraphLayoutSugiyama_Private::DFSVisit(NodeIdx, Model, NodeColor);
		}
	}

	FArcRecipeGraphLayout::RebuildAdjacency(Model);
}

void RecomputeLayers(FArcRecipeLayoutModel& Model)
{
	const int32 NumNodes = Model.Nodes.Num();
	if (NumNodes == 0)
	{
		return;
	}

	TArray<int32> NewLayers = ArcRecipeGraphLayoutSugiyama_Private::ComputeLongestPathLayers(Model);

	// Update per-node Layer field.
	int32 MaxLayer = 0;
	for (int32 i = 0; i < NumNodes; ++i)
	{
		Model.Nodes[i].Layer = NewLayers[i];
		if (NewLayers[i] > MaxLayer)
		{
			MaxLayer = NewLayers[i];
		}
	}

	// Rebuild Layers arrays preserving relative order of nodes within each layer.
	// Nodes that already existed in the same layer keep their relative insertion order
	// (iterate by old Layers order, then add nodes not present in any layer yet).
	//
	// Strategy: rebuild from old Layers arrays in row-order, then append any nodes
	// missing from the traversal (isolated nodes not in any old layer).
	TArray<TArray<int32>> NewLayerArrays;
	NewLayerArrays.SetNum(MaxLayer + 1);

	// Track which nodes have been placed.
	TArray<bool> bPlaced;
	bPlaced.SetNumZeroed(NumNodes);

	// First pass: walk existing Layers in order; place each node into its new layer.
	for (const TArray<int32>& OldLayer : Model.Layers)
	{
		for (int32 NodeIdx : OldLayer)
		{
			if (!Model.Nodes.IsValidIndex(NodeIdx))
			{
				continue;
			}
			if (bPlaced[NodeIdx])
			{
				continue;
			}
			const int32 TargetLayer = Model.Nodes[NodeIdx].Layer;
			if (NewLayerArrays.IsValidIndex(TargetLayer))
			{
				NewLayerArrays[TargetLayer].Add(NodeIdx);
				bPlaced[NodeIdx] = true;
			}
		}
	}

	// Second pass: any node not yet placed (new dummies or nodes not in old Layers).
	for (int32 NodeIdx = 0; NodeIdx < NumNodes; ++NodeIdx)
	{
		if (bPlaced[NodeIdx])
		{
			continue;
		}
		const int32 TargetLayer = Model.Nodes[NodeIdx].Layer;
		if (NewLayerArrays.IsValidIndex(TargetLayer))
		{
			NewLayerArrays[TargetLayer].Add(NodeIdx);
			bPlaced[NodeIdx] = true;
		}
	}

	Model.Layers = MoveTemp(NewLayerArrays);
}

void InsertDummies(FArcRecipeLayoutModel& Model)
{
	const int32 NumEdgesBeforeInsertion = Model.Edges.Num();
	if (NumEdgesBeforeInsertion == 0)
	{
		return;
	}

	// Two-pass replacement — collect new chains and removal indices first,
	// then bulk-remove old long edges.

	// For each original edge index that spans >1 layer, collect the replacement chain.
	// Replacement chain: array of single-span edges from[i] -> to[i].
	struct FEdgeChain
	{
		int32 OriginalEdgeIndex;
		TArray<FArcRecipeLayoutEdge> ReplacementEdges;
		TArray<int32> NewDummyNodeIndices; // model indices of the dummies to be added to Layers
		TArray<int32> NewDummyLayers;      // which layer each dummy belongs to
	};

	TArray<FEdgeChain> Chains;

	for (int32 EdgeIdx = 0; EdgeIdx < NumEdgesBeforeInsertion; ++EdgeIdx)
	{
		const FArcRecipeLayoutEdge& Edge = Model.Edges[EdgeIdx];

		if (!Model.Nodes.IsValidIndex(Edge.From) || !Model.Nodes.IsValidIndex(Edge.To))
		{
			continue;
		}

		const int32 FromLayer = Model.Nodes[Edge.From].Layer;
		const int32 ToLayer   = Model.Nodes[Edge.To].Layer;
		const int32 Span      = ToLayer - FromLayer;

		if (Span <= 1)
		{
			// Edge already spans at most one layer — no dummy needed.
			continue;
		}

		// This edge spans (Span) layers — needs (Span - 1) dummies.
		FEdgeChain Chain;
		Chain.OriginalEdgeIndex = EdgeIdx;

		// The first dummy node index will be Model.Nodes.Num() + (dummies already staged
		// by previous chains).  We compute this after collecting all chains.
		// Store layer offsets for now; node indices are patched below.
		const int32 NumDummies = Span - 1;
		Chain.NewDummyLayers.SetNum(NumDummies);
		for (int32 d = 0; d < NumDummies; ++d)
		{
			Chain.NewDummyLayers[d] = FromLayer + d + 1;
		}

		Chains.Add(MoveTemp(Chain));
	}

	if (Chains.Num() == 0)
	{
		return;
	}

	// Assign node indices for dummies: they are appended to Model.Nodes after
	// the current last node.  Chains are processed in iteration order so indices
	// are deterministic.
	int32 NextDummyIdx = Model.Nodes.Num();

	for (FEdgeChain& Chain : Chains)
	{
		const FArcRecipeLayoutEdge& OrigEdge = Model.Edges[Chain.OriginalEdgeIndex];
		const int32 NumDummies = Chain.NewDummyLayers.Num();

		Chain.NewDummyNodeIndices.SetNum(NumDummies);
		for (int32 d = 0; d < NumDummies; ++d)
		{
			Chain.NewDummyNodeIndices[d] = NextDummyIdx;
			++NextDummyIdx;
		}

		// Build replacement edge chain: Original.From → D0 → D1 → … → D(k-1) → Original.To.
		Chain.ReplacementEdges.Reserve(NumDummies + 1);

		// First edge: From → D0.
		FArcRecipeLayoutEdge FirstEdge;
		FirstEdge.From = OrigEdge.From;
		FirstEdge.To   = Chain.NewDummyNodeIndices[0];
		Chain.ReplacementEdges.Add(FirstEdge);

		// Middle edges: Di → Di+1.
		for (int32 d = 0; d + 1 < NumDummies; ++d)
		{
			FArcRecipeLayoutEdge MiddleEdge;
			MiddleEdge.From = Chain.NewDummyNodeIndices[d];
			MiddleEdge.To   = Chain.NewDummyNodeIndices[d + 1];
			Chain.ReplacementEdges.Add(MiddleEdge);
		}

		// Last edge: D(k-1) → To.
		FArcRecipeLayoutEdge LastEdge;
		LastEdge.From = Chain.NewDummyNodeIndices[NumDummies - 1];
		LastEdge.To   = OrigEdge.To;
		Chain.ReplacementEdges.Add(LastEdge);
	}

	// Append dummy nodes to Model.Nodes.
	for (const FEdgeChain& Chain : Chains)
	{
		for (int32 d = 0; d < Chain.NewDummyNodeIndices.Num(); ++d)
		{
			FArcRecipeLayoutNode Dummy;
			Dummy.bDummy = true;
			Dummy.Size   = FVector2D(8.0f, 8.0f);
			Dummy.Layer  = Chain.NewDummyLayers[d];
			Model.Nodes.Add(Dummy);
		}
	}

	// Ensure Layers arrays are large enough to hold all layers that dummies belong to.
	// (RecomputeLayers already sized Layers; but check in case of edge cases.)
	for (const FEdgeChain& Chain : Chains)
	{
		for (int32 LayerIdx : Chain.NewDummyLayers)
		{
			while (Model.Layers.Num() <= LayerIdx)
			{
				Model.Layers.AddDefaulted();
			}
		}
	}

	// Append dummies to their Layers arrays in chain/edge-iteration order.
	for (const FEdgeChain& Chain : Chains)
	{
		for (int32 d = 0; d < Chain.NewDummyNodeIndices.Num(); ++d)
		{
			const int32 DummyNodeIdx = Chain.NewDummyNodeIndices[d];
			const int32 DummyLayer   = Chain.NewDummyLayers[d];
			Model.Layers[DummyLayer].Add(DummyNodeIdx);
		}
	}

	// Two-pass edge replacement:
	// Pass 1 — collect indices to remove (in reverse order for stable removal).
	TArray<int32> IndicesToRemove;
	IndicesToRemove.Reserve(Chains.Num());
	for (const FEdgeChain& Chain : Chains)
	{
		IndicesToRemove.Add(Chain.OriginalEdgeIndex);
	}

	// Sort descending so we can remove from back to front without shifting issues.
	IndicesToRemove.Sort([](int32 A, int32 B) { return A > B; });

	// Pass 2 — remove old long edges (back-to-front, single-element removes are fine
	// here because indices are sorted descending and each removal doesn't affect lower indices).
	for (int32 RemoveIdx : IndicesToRemove)
	{
		Model.Edges.RemoveAt(RemoveIdx);
	}

	// Append all replacement edges.
	for (const FEdgeChain& Chain : Chains)
	{
		for (const FArcRecipeLayoutEdge& NewEdge : Chain.ReplacementEdges)
		{
			Model.Edges.Add(NewEdge);
		}
	}

	FArcRecipeGraphLayout::RebuildAdjacency(Model);
}

void GreedySwitch(FArcRecipeLayoutModel& Model)
{
	if (Model.Layers.Num() < 2)
	{
		return;
	}

	bool bImproved = true;
	while (bImproved)
	{
		bImproved = false;

		const int32 NumLayers = Model.Layers.Num();
		for (int32 L = 0; L < NumLayers; ++L)
		{
			TArray<int32>& Layer = Model.Layers[L];
			const int32 LayerSize = Layer.Num();

			if (LayerSize < 2)
			{
				continue;
			}

			// Try swapping each adjacent pair within the layer.
			for (int32 i = 0; i + 1 < LayerSize; ++i)
			{
				const int32 CrossingsBefore = ArcRecipeGraphLayoutLayered::CountCrossings(Model);

				// Swap adjacent nodes i and i+1.
				TArray<int32>::ElementType Temp = Layer[i];
				Layer[i]     = Layer[i + 1];
				Layer[i + 1] = Temp;

				const int32 CrossingsAfter = ArcRecipeGraphLayoutLayered::CountCrossings(Model);

				if (CrossingsAfter < CrossingsBefore)
				{
					// Swap was beneficial — keep it and mark that we improved.
					bImproved = true;
				}
				else
				{
					// Swap made no improvement — revert.
					Temp         = Layer[i];
					Layer[i]     = Layer[i + 1];
					Layer[i + 1] = Temp;
				}
			}
		}
	}
}

void ApplySugiyama(FArcRecipeLayoutModel& Model)
{
	RemoveCycles(Model);
	RecomputeLayers(Model);
	InsertDummies(Model);
	ArcRecipeGraphLayoutLayered::MinimizeCrossings(Model);
	GreedySwitch(Model);

	ArcRecipeGraphLayoutSugiyama_Private::BrandesKoepfAssign(Model);
}

} // namespace ArcRecipeGraphLayoutSugiyama
