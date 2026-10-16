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

// ArcRecipeGraphLayoutTests.cpp
// CQTest suite for pure-model layout algorithm functions.
// No assets, no UObjects — all fixtures are hand-built FArcRecipeLayoutModel structs.

#if WITH_EDITOR

#include "CQTest.h"

#include "RecipeGraph/ArcRecipeGraphLayoutModel.h"
#include "RecipeGraph/ArcRecipeGraphLayout.h"
#include "RecipeGraph/ArcRecipeGraphLayoutLayered.h"
#include "RecipeGraph/ArcRecipeGraphLayoutSugiyama.h"

// ---------------------------------------------------------------------------
// File-scoped helpers — unique namespace required by project rules.
// ---------------------------------------------------------------------------

namespace ArcRecipeGraphLayoutTests_Private
{

/**
 * Add a node to the model with the given layer and size.
 * Returns the new node's index.
 */
static int32 AddNode(FArcRecipeLayoutModel& Model, int32 Layer, FVector2D Size)
{
	const int32 Idx = Model.Nodes.Num();
	FArcRecipeLayoutNode Node;
	Node.Layer = Layer;
	Node.Size  = Size;
	Model.Nodes.Add(Node);
	return Idx;
}

/**
 * Add a directed edge From -> To and record it in the model.
 * Does NOT call RebuildAdjacency; the caller must do that once all edges are added.
 */
static void AddEdge(FArcRecipeLayoutModel& Model, int32 From, int32 To)
{
	FArcRecipeLayoutEdge Edge;
	Edge.From = From;
	Edge.To   = To;
	Model.Edges.Add(Edge);
}

/**
 * Ensure Model.Layers contains at least (MaxLayer + 1) entries and that each
 * node is listed in its Layer's array.  Does NOT call RebuildAdjacency.
 */
static void BuildLayersFromNodes(FArcRecipeLayoutModel& Model)
{
	// Find the highest layer index used.
	int32 MaxLayer = 0;
	for (const FArcRecipeLayoutNode& N : Model.Nodes)
	{
		if (N.Layer > MaxLayer)
		{
			MaxLayer = N.Layer;
		}
	}

	Model.Layers.SetNum(MaxLayer + 1);
	for (int32 LayerIdx = 0; LayerIdx <= MaxLayer; ++LayerIdx)
	{
		Model.Layers[LayerIdx].Empty();
	}

	const int32 NumNodes = Model.Nodes.Num();
	for (int32 i = 0; i < NumNodes; ++i)
	{
		const int32 L = Model.Nodes[i].Layer;
		if (Model.Layers.IsValidIndex(L))
		{
			Model.Layers[L].Add(i);
		}
	}
}

/**
 * Kahn's algorithm: returns true if the edge graph of Model is acyclic
 * (all nodes can be sorted topologically).
 */
static bool IsAcyclic(const FArcRecipeLayoutModel& Model)
{
	const int32 NumNodes = Model.Nodes.Num();
	TArray<int32> InDegree;
	InDegree.SetNumZeroed(NumNodes);

	for (const FArcRecipeLayoutEdge& E : Model.Edges)
	{
		if (InDegree.IsValidIndex(E.To))
		{
			++InDegree[E.To];
		}
	}

	TArray<int32> Queue;
	Queue.Reserve(NumNodes);
	for (int32 i = 0; i < NumNodes; ++i)
	{
		if (InDegree[i] == 0)
		{
			Queue.Add(i);
		}
	}

	int32 Processed = 0;
	int32 Head = 0;
	while (Head < Queue.Num())
	{
		const int32 NodeIdx = Queue[Head];
		++Head;
		++Processed;

		for (const FArcRecipeLayoutEdge& E : Model.Edges)
		{
			if (E.From != NodeIdx)
			{
				continue;
			}
			if (!InDegree.IsValidIndex(E.To))
			{
				continue;
			}
			--InDegree[E.To];
			if (InDegree[E.To] == 0)
			{
				Queue.Add(E.To);
			}
		}
	}

	return Processed == NumNodes;
}

} // namespace ArcRecipeGraphLayoutTests_Private

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcRecipeGraphLayoutTest, "Arc.ArcCraft.RecipeGraphLayout")
{
	// =========================================================================
	// Test 1: CountCrossings exact count
	//
	// 2 layers, 2 nodes each.
	//   X-pattern:   A0->B1, A1->B0 => exactly 1 crossing.
	//   Parallel:    A0->B0, A1->B1 => 0 crossings.
	// =========================================================================
	TEST_METHOD(CountCrossings_ExactCount)
	{
		using namespace ArcRecipeGraphLayoutTests_Private;

		// --- X-pattern fixture ---
		FArcRecipeLayoutModel CrossModel;
		// Layer 0: nodes 0,1. Layer 1: nodes 2,3.
		const int32 A0 = AddNode(CrossModel, 0, FVector2D(100.0f, 64.0f));
		const int32 A1 = AddNode(CrossModel, 0, FVector2D(100.0f, 64.0f));
		const int32 B0 = AddNode(CrossModel, 1, FVector2D(100.0f, 64.0f));
		const int32 B1 = AddNode(CrossModel, 1, FVector2D(100.0f, 64.0f));

		AddEdge(CrossModel, A0, B1); // A0 -> B1 (ordinal 0 -> ordinal 1)
		AddEdge(CrossModel, A1, B0); // A1 -> B0 (ordinal 1 -> ordinal 0) — crossing!

		BuildLayersFromNodes(CrossModel);
		FArcRecipeGraphLayout::RebuildAdjacency(CrossModel);

		const int32 XCrossings = ArcRecipeGraphLayoutLayered::CountCrossings(CrossModel);
		ASSERT_THAT(AreEqual(1, XCrossings, TEXT("X-pattern must have exactly 1 crossing")));

		// --- Parallel fixture ---
		FArcRecipeLayoutModel ParallelModel;
		const int32 PA0 = AddNode(ParallelModel, 0, FVector2D(100.0f, 64.0f));
		const int32 PA1 = AddNode(ParallelModel, 0, FVector2D(100.0f, 64.0f));
		const int32 PB0 = AddNode(ParallelModel, 1, FVector2D(100.0f, 64.0f));
		const int32 PB1 = AddNode(ParallelModel, 1, FVector2D(100.0f, 64.0f));

		AddEdge(ParallelModel, PA0, PB0); // A0 -> B0 (ordinal 0 -> ordinal 0)
		AddEdge(ParallelModel, PA1, PB1); // A1 -> B1 (ordinal 1 -> ordinal 1) — no crossing

		BuildLayersFromNodes(ParallelModel);
		FArcRecipeGraphLayout::RebuildAdjacency(ParallelModel);

		const int32 ParallelCrossings = ArcRecipeGraphLayoutLayered::CountCrossings(ParallelModel);
		ASSERT_THAT(AreEqual(0, ParallelCrossings, TEXT("Parallel-pattern must have 0 crossings")));
	}

	// =========================================================================
	// Test 2: MinimizeCrossings reduces crossings
	//
	// 3-layer fixture deliberately misordered so layer 1 is reversed
	// relative to layer 0.  After minimization:
	//   a) crossings(after) <= crossings(before)
	//   b) the simple X-pattern (2 layers, 2 nodes) resolves to 0 crossings
	//   c) two identical runs produce identical Layers orderings (determinism)
	// =========================================================================
	TEST_METHOD(MinimizeCrossings_ReducesAndDeterministic)
	{
		using namespace ArcRecipeGraphLayoutTests_Private;

		// --- X-pattern resolves to 0 ---
		FArcRecipeLayoutModel XModel;
		const int32 XA0 = AddNode(XModel, 0, FVector2D(100.0f, 64.0f));
		const int32 XA1 = AddNode(XModel, 0, FVector2D(100.0f, 64.0f));
		const int32 XB0 = AddNode(XModel, 1, FVector2D(100.0f, 64.0f));
		const int32 XB1 = AddNode(XModel, 1, FVector2D(100.0f, 64.0f));
		AddEdge(XModel, XA0, XB1);
		AddEdge(XModel, XA1, XB0);
		BuildLayersFromNodes(XModel);
		FArcRecipeGraphLayout::RebuildAdjacency(XModel);

		ArcRecipeGraphLayoutLayered::MinimizeCrossings(XModel);
		const int32 AfterX = ArcRecipeGraphLayoutLayered::CountCrossings(XModel);
		ASSERT_THAT(AreEqual(0, AfterX, TEXT("X-pattern must resolve to 0 crossings after MinimizeCrossings")));

		// --- 3-layer deliberately crossed fixture ---
		// Layer 0: N0, N1
		// Layer 1: N2, N3  (deliberately reversed: N3 connects from N0, N2 from N1 -> crossing)
		// Layer 2: N4, N5
		FArcRecipeLayoutModel ThreeLayerModel;
		const int32 N0 = AddNode(ThreeLayerModel, 0, FVector2D(100.0f, 64.0f));
		const int32 N1 = AddNode(ThreeLayerModel, 0, FVector2D(100.0f, 64.0f));
		const int32 N2 = AddNode(ThreeLayerModel, 1, FVector2D(100.0f, 64.0f));
		const int32 N3 = AddNode(ThreeLayerModel, 1, FVector2D(100.0f, 64.0f));
		const int32 N4 = AddNode(ThreeLayerModel, 2, FVector2D(100.0f, 64.0f));
		const int32 N5 = AddNode(ThreeLayerModel, 2, FVector2D(100.0f, 64.0f));
		// Crossed: N0->N3, N1->N2 (layer0->layer1 crossing)
		//          N3->N4, N2->N5 (layer1->layer2 — additional crossing due to reversed ordering)
		AddEdge(ThreeLayerModel, N0, N3);
		AddEdge(ThreeLayerModel, N1, N2);
		AddEdge(ThreeLayerModel, N3, N4);
		AddEdge(ThreeLayerModel, N2, N5);
		BuildLayersFromNodes(ThreeLayerModel);
		FArcRecipeGraphLayout::RebuildAdjacency(ThreeLayerModel);

		const int32 Before = ArcRecipeGraphLayoutLayered::CountCrossings(ThreeLayerModel);
		ArcRecipeGraphLayoutLayered::MinimizeCrossings(ThreeLayerModel);
		const int32 After = ArcRecipeGraphLayoutLayered::CountCrossings(ThreeLayerModel);
		ASSERT_THAT(IsTrue(After <= Before, TEXT("MinimizeCrossings must not increase crossings")));

		// --- Determinism: two identical runs must produce identical Layers ---
		FArcRecipeLayoutModel DetModelA;
		const int32 DA0 = AddNode(DetModelA, 0, FVector2D(100.0f, 64.0f));
		const int32 DA1 = AddNode(DetModelA, 0, FVector2D(100.0f, 64.0f));
		const int32 DB0 = AddNode(DetModelA, 1, FVector2D(100.0f, 64.0f));
		const int32 DB1 = AddNode(DetModelA, 1, FVector2D(100.0f, 64.0f));
		AddEdge(DetModelA, DA0, DB1);
		AddEdge(DetModelA, DA1, DB0);
		BuildLayersFromNodes(DetModelA);
		FArcRecipeGraphLayout::RebuildAdjacency(DetModelA);

		FArcRecipeLayoutModel DetModelB = DetModelA;

		ArcRecipeGraphLayoutLayered::MinimizeCrossings(DetModelA);
		ArcRecipeGraphLayoutLayered::MinimizeCrossings(DetModelB);

		bool bLayersMatch = true;
		if (DetModelA.Layers.Num() != DetModelB.Layers.Num())
		{
			bLayersMatch = false;
		}
		else
		{
			const int32 NumLayersA = DetModelA.Layers.Num();
			for (int32 L = 0; L < NumLayersA && bLayersMatch; ++L)
			{
				if (DetModelA.Layers[L].Num() != DetModelB.Layers[L].Num())
				{
					bLayersMatch = false;
					break;
				}
				const int32 LayerSize = DetModelA.Layers[L].Num();
				for (int32 I = 0; I < LayerSize; ++I)
				{
					if (DetModelA.Layers[L][I] != DetModelB.Layers[L][I])
					{
						bLayersMatch = false;
						break;
					}
				}
			}
		}
		ASSERT_THAT(IsTrue(bLayersMatch, TEXT("MinimizeCrossings must be deterministic: two identical runs must produce identical Layers")));
	}

	// =========================================================================
	// Test 3: AssignCoordinates no overlap
	//
	// One layer with 3 nodes of mixed heights (64, 200, 64).
	// After AssignCoordinates, the vertical gap between consecutive same-layer
	// nodes must be >= 24 (bottom of upper node to top of lower node).
	// =========================================================================
	TEST_METHOD(AssignCoordinates_NoOverlap_MixedHeights)
	{
		using namespace ArcRecipeGraphLayoutTests_Private;

		FArcRecipeLayoutModel Model;
		AddNode(Model, 0, FVector2D(120.0f,  64.0f));
		AddNode(Model, 0, FVector2D(120.0f, 200.0f));
		AddNode(Model, 0, FVector2D(120.0f,  64.0f));
		BuildLayersFromNodes(Model);
		FArcRecipeGraphLayout::RebuildAdjacency(Model);

		ArcRecipeGraphLayoutLayered::AssignCoordinates(Model);

		// Check gap between each consecutive pair in layer 0.
		const TArray<int32>& Layer0 = Model.Layers[0];
		const int32 LayerSize = Layer0.Num();
		for (int32 I = 0; I + 1 < LayerSize; ++I)
		{
			const int32 UpperIdx = Layer0[I];
			const int32 LowerIdx = Layer0[I + 1];
			const FArcRecipeLayoutNode& Upper = Model.Nodes[UpperIdx];
			const FArcRecipeLayoutNode& Lower = Model.Nodes[LowerIdx];

			const float UpperBottom = Upper.Pos.Y + Upper.Size.Y;
			const float LowerTop    = Lower.Pos.Y;
			const float Gap         = LowerTop - UpperBottom;

			ASSERT_THAT(IsTrue(Gap >= 23.9f, TEXT("Gap between consecutive same-layer nodes must be >= 24")));
		}
	}

	// =========================================================================
	// Test 4: RemoveCycles
	//
	// 3-node cycle: A->B->C->A.
	// After RemoveCycles:
	//   a) exactly one edge has bReversed == true
	//   b) the resulting edge graph is acyclic (Kahn's sort processes all nodes)
	// =========================================================================
	TEST_METHOD(RemoveCycles_CycleBecomesAcyclic)
	{
		using namespace ArcRecipeGraphLayoutTests_Private;

		FArcRecipeLayoutModel Model;
		const int32 A = AddNode(Model, 0, FVector2D(100.0f, 64.0f));
		const int32 B = AddNode(Model, 0, FVector2D(100.0f, 64.0f));
		const int32 C = AddNode(Model, 0, FVector2D(100.0f, 64.0f));
		AddEdge(Model, A, B); // A -> B
		AddEdge(Model, B, C); // B -> C
		AddEdge(Model, C, A); // C -> A  (back-edge that closes the cycle)

		BuildLayersFromNodes(Model);
		FArcRecipeGraphLayout::RebuildAdjacency(Model);

		ArcRecipeGraphLayoutSugiyama::RemoveCycles(Model);

		// Count reversed edges.
		int32 ReversedCount = 0;
		for (const FArcRecipeLayoutEdge& E : Model.Edges)
		{
			if (E.bReversed)
			{
				++ReversedCount;
			}
		}
		ASSERT_THAT(AreEqual(1, ReversedCount, TEXT("Exactly one edge must be reversed to break the 3-cycle")));

		// The resulting graph must be acyclic.
		ASSERT_THAT(IsTrue(IsAcyclic(Model), TEXT("Graph must be acyclic after RemoveCycles")));
	}

	// =========================================================================
	// Test 5: InsertDummies
	//
	// Fixture:
	//   A in layer 0, B in layer 3, edge A->B (spans 3 layers).
	//   Plus filler nodes in layers 1, 2 so Layers arrays exist for all layers.
	//
	// After InsertDummies:
	//   a) Nodes.Num() == NumRealNodes + 2  (2 dummy nodes for a 3-layer span)
	//   b) Both new nodes have bDummy == true
	//   c) Chain A -> D0 -> D1 -> B connectivity via OutNodes (using RebuildAdjacency)
	//   d) No edge spans > 1 layer
	// =========================================================================
	TEST_METHOD(InsertDummies_LongEdge_TwoDummies)
	{
		using namespace ArcRecipeGraphLayoutTests_Private;

		FArcRecipeLayoutModel Model;
		const int32 NodeA    = AddNode(Model, 0, FVector2D(100.0f, 64.0f));
		const int32 Filler1  = AddNode(Model, 1, FVector2D(100.0f, 64.0f)); // filler so layer 1 exists
		const int32 Filler2  = AddNode(Model, 2, FVector2D(100.0f, 64.0f)); // filler so layer 2 exists
		const int32 NodeB    = AddNode(Model, 3, FVector2D(100.0f, 64.0f));

		AddEdge(Model, NodeA, NodeB); // spans 3 layers — needs 2 dummies

		BuildLayersFromNodes(Model);
		FArcRecipeGraphLayout::RebuildAdjacency(Model);

		// Run the full upstream Sugiyama pipeline steps that InsertDummies depends on.
		ArcRecipeGraphLayoutSugiyama::RemoveCycles(Model);
		ArcRecipeGraphLayoutSugiyama::RecomputeLayers(Model);

		const int32 NumRealNodes = Model.Nodes.Num(); // 4 real nodes at this point
		Model.NumRealNodes = NumRealNodes;

		ArcRecipeGraphLayoutSugiyama::InsertDummies(Model);

		// Should have 2 dummies appended.
		const int32 ExpectedTotal = NumRealNodes + 2;
		ASSERT_THAT(AreEqual(ExpectedTotal, Model.Nodes.Num(), TEXT("InsertDummies must append exactly 2 dummy nodes for a 3-layer span")));

		// Both appended nodes must have bDummy == true.
		for (int32 DummyIdx = NumRealNodes; DummyIdx < Model.Nodes.Num(); ++DummyIdx)
		{
			ASSERT_THAT(IsTrue(Model.Nodes[DummyIdx].bDummy, TEXT("Appended node must have bDummy == true")));
		}

		// Chain: NodeA -> D0 -> D1 -> NodeB via OutNodes.
		// NodeA's OutNodes must contain D0.
		const FArcRecipeLayoutNode& NA = Model.Nodes[NodeA];
		ASSERT_THAT(IsTrue(NA.OutNodes.Num() > 0, TEXT("NodeA must have at least one out-neighbor after InsertDummies")));
		const int32 D0Idx = NA.OutNodes[0];
		ASSERT_THAT(IsTrue(Model.Nodes.IsValidIndex(D0Idx), TEXT("D0 index must be valid")));
		ASSERT_THAT(IsTrue(Model.Nodes[D0Idx].bDummy, TEXT("D0 must be a dummy node")));

		// D0's OutNodes must contain D1.
		const FArcRecipeLayoutNode& D0 = Model.Nodes[D0Idx];
		ASSERT_THAT(IsTrue(D0.OutNodes.Num() > 0, TEXT("D0 must have at least one out-neighbor")));
		const int32 D1Idx = D0.OutNodes[0];
		ASSERT_THAT(IsTrue(Model.Nodes.IsValidIndex(D1Idx), TEXT("D1 index must be valid")));
		ASSERT_THAT(IsTrue(Model.Nodes[D1Idx].bDummy, TEXT("D1 must be a dummy node")));

		// D1's OutNodes must contain NodeB.
		const FArcRecipeLayoutNode& D1 = Model.Nodes[D1Idx];
		ASSERT_THAT(IsTrue(D1.OutNodes.Num() > 0, TEXT("D1 must have at least one out-neighbor")));
		const int32 ChainEndIdx = D1.OutNodes[0];
		ASSERT_THAT(AreEqual(NodeB, ChainEndIdx, TEXT("Chain must terminate at NodeB")));

		// No edge must span more than 1 layer.
		for (const FArcRecipeLayoutEdge& E : Model.Edges)
		{
			if (!Model.Nodes.IsValidIndex(E.From) || !Model.Nodes.IsValidIndex(E.To))
			{
				continue;
			}
			const int32 Span = Model.Nodes[E.To].Layer - Model.Nodes[E.From].Layer;
			ASSERT_THAT(IsTrue(FMath::Abs(Span) <= 1, TEXT("No edge must span more than 1 layer after InsertDummies")));
		}
	}

	// =========================================================================
	// Test 6: ApplySugiyama — straight chain + determinism + no overlap
	//
	// Straight chain: A->B->C, one node per layer, equal sizes 100x64.
	// After ApplySugiyama:
	//   a) All three center-Y values (Pos.Y + Size.Y * 0.5) are equal within 0.5f.
	//   b) Two runs on identical fixtures produce identical positions.
	//
	// 2-per-layer variant: same-layer nodes do not overlap vertically (gap >= 24).
	// =========================================================================
	TEST_METHOD(ApplySugiyama_StraightChainAndDeterminism)
	{
		using namespace ArcRecipeGraphLayoutTests_Private;

		// --- Straight chain A->B->C ---
		FArcRecipeLayoutModel ChainModel;
		ChainModel.NumRealNodes = 3;
		const int32 NA = AddNode(ChainModel, 0, FVector2D(100.0f, 64.0f));
		const int32 NB = AddNode(ChainModel, 1, FVector2D(100.0f, 64.0f));
		const int32 NC = AddNode(ChainModel, 2, FVector2D(100.0f, 64.0f));
		AddEdge(ChainModel, NA, NB);
		AddEdge(ChainModel, NB, NC);
		BuildLayersFromNodes(ChainModel);
		FArcRecipeGraphLayout::RebuildAdjacency(ChainModel);

		ArcRecipeGraphLayoutSugiyama::ApplySugiyama(ChainModel);

		// After ApplySugiyama all three nodes should have been processed.
		// Verify center-Y values are equal within 0.5f.
		const float CenterA = ChainModel.Nodes[NA].Pos.Y + ChainModel.Nodes[NA].Size.Y * 0.5f;
		const float CenterB = ChainModel.Nodes[NB].Pos.Y + ChainModel.Nodes[NB].Size.Y * 0.5f;
		const float CenterC = ChainModel.Nodes[NC].Pos.Y + ChainModel.Nodes[NC].Size.Y * 0.5f;
		ASSERT_THAT(IsTrue(FMath::Abs(CenterA - CenterB) <= 0.5f, TEXT("Straight chain: A and B must share center-Y within 0.5f")));
		ASSERT_THAT(IsTrue(FMath::Abs(CenterB - CenterC) <= 0.5f, TEXT("Straight chain: B and C must share center-Y within 0.5f")));

		// --- Determinism: two identical runs ---
		FArcRecipeLayoutModel DetA;
		DetA.NumRealNodes = 3;
		const int32 DA_A = AddNode(DetA, 0, FVector2D(100.0f, 64.0f));
		const int32 DA_B = AddNode(DetA, 1, FVector2D(100.0f, 64.0f));
		const int32 DA_C = AddNode(DetA, 2, FVector2D(100.0f, 64.0f));
		AddEdge(DetA, DA_A, DA_B);
		AddEdge(DetA, DA_B, DA_C);
		BuildLayersFromNodes(DetA);
		FArcRecipeGraphLayout::RebuildAdjacency(DetA);

		FArcRecipeLayoutModel DetB = DetA;

		ArcRecipeGraphLayoutSugiyama::ApplySugiyama(DetA);
		ArcRecipeGraphLayoutSugiyama::ApplySugiyama(DetB);

		for (int32 I = 0; I < 3; ++I)
		{
			ASSERT_THAT(IsTrue(FMath::Abs(DetA.Nodes[I].Pos.X - DetB.Nodes[I].Pos.X) <= 0.5f,
				TEXT("ApplySugiyama must be deterministic: Pos.X differs between runs")));
			ASSERT_THAT(IsTrue(FMath::Abs(DetA.Nodes[I].Pos.Y - DetB.Nodes[I].Pos.Y) <= 0.5f,
				TEXT("ApplySugiyama must be deterministic: Pos.Y differs between runs")));
		}

		// --- 2-per-layer no-overlap variant ---
		// 2 layers, 2 nodes each, edges A0->B0, A1->B1 (no crossings).
		FArcRecipeLayoutModel TwoPerLayer;
		TwoPerLayer.NumRealNodes = 4;
		const int32 TP_A0 = AddNode(TwoPerLayer, 0, FVector2D(100.0f, 64.0f));
		const int32 TP_A1 = AddNode(TwoPerLayer, 0, FVector2D(100.0f, 64.0f));
		const int32 TP_B0 = AddNode(TwoPerLayer, 1, FVector2D(100.0f, 64.0f));
		const int32 TP_B1 = AddNode(TwoPerLayer, 1, FVector2D(100.0f, 64.0f));
		AddEdge(TwoPerLayer, TP_A0, TP_B0);
		AddEdge(TwoPerLayer, TP_A1, TP_B1);
		BuildLayersFromNodes(TwoPerLayer);
		FArcRecipeGraphLayout::RebuildAdjacency(TwoPerLayer);

		ArcRecipeGraphLayoutSugiyama::ApplySugiyama(TwoPerLayer);

		// For each layer, check that no two nodes overlap vertically (gap >= 24).
		for (const TArray<int32>& LayerArr : TwoPerLayer.Layers)
		{
			const int32 LSize = LayerArr.Num();
			for (int32 I = 0; I + 1 < LSize; ++I)
			{
				// Check that nodes are sorted by Y (not guaranteed by BK but separation is).
				// We check the gap between consecutive nodes sorted by Pos.Y.
				TArray<int32> Sorted = LayerArr;
				Sorted.Sort([&TwoPerLayer](int32 X, int32 Y)
				{
					return TwoPerLayer.Nodes[X].Pos.Y < TwoPerLayer.Nodes[Y].Pos.Y;
				});

				const FArcRecipeLayoutNode& Upper = TwoPerLayer.Nodes[Sorted[I]];
				const FArcRecipeLayoutNode& Lower = TwoPerLayer.Nodes[Sorted[I + 1]];
				const float Gap = Lower.Pos.Y - (Upper.Pos.Y + Upper.Size.Y);
				ASSERT_THAT(IsTrue(Gap >= 23.9f, TEXT("ApplySugiyama 2-per-layer: same-layer nodes must not overlap (gap >= 24)")));
			}
		}
	}

	// =========================================================================
	// Test 7: StraightenY separation
	//
	// 2-layer fixture where BOTH layer-1 nodes connect to the SAME layer-0 node
	// (median pull tries to collapse them together).
	// After AssignCoordinates + StraightenY, the two layer-1 nodes must keep
	// >= 24 gap edge-to-edge.
	// =========================================================================
	TEST_METHOD(StraightenY_MaintainsSeparation)
	{
		using namespace ArcRecipeGraphLayoutTests_Private;

		FArcRecipeLayoutModel Model;
		// Layer 0: single node that both layer-1 nodes connect FROM.
		const int32 Src  = AddNode(Model, 0, FVector2D(100.0f, 64.0f));
		// Layer 1: two nodes, same height.
		const int32 Dst0 = AddNode(Model, 1, FVector2D(100.0f, 64.0f));
		const int32 Dst1 = AddNode(Model, 1, FVector2D(100.0f, 64.0f));

		AddEdge(Model, Src, Dst0);
		AddEdge(Model, Src, Dst1);

		BuildLayersFromNodes(Model);
		FArcRecipeGraphLayout::RebuildAdjacency(Model);

		ArcRecipeGraphLayoutLayered::AssignCoordinates(Model);
		ArcRecipeGraphLayoutLayered::StraightenY(Model);

		// Sort layer-1 nodes by Pos.Y to reliably measure gap.
		TArray<int32> Layer1Sorted = { Dst0, Dst1 };
		Layer1Sorted.Sort([&Model](int32 A, int32 B)
		{
			return Model.Nodes[A].Pos.Y < Model.Nodes[B].Pos.Y;
		});

		const FArcRecipeLayoutNode& Upper = Model.Nodes[Layer1Sorted[0]];
		const FArcRecipeLayoutNode& Lower = Model.Nodes[Layer1Sorted[1]];
		const float Gap = Lower.Pos.Y - (Upper.Pos.Y + Upper.Size.Y);
		ASSERT_THAT(IsTrue(Gap >= 23.9f, TEXT("StraightenY: two layer-1 nodes sharing one source must keep >= 24 gap edge-to-edge")));
	}
};

#endif // WITH_EDITOR
