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

#include "RecipeGraph/ArcRecipeGraphLayout.h"
#include "RecipeGraph/ArcRecipeGraph.h"
#include "RecipeGraph/ArcRecipeGraphNode.h"
#include "RecipeGraph/ArcRecipeGraphLayoutLayered.h"
#include "RecipeGraph/ArcRecipeGraphLayoutSugiyama.h"
#include "RecipeGraph/ArcRecipeGraphLayoutForce.h"

#include "EdGraph/EdGraphPin.h"
#include "SGraphNode.h"

namespace ArcRecipeGraphLayout_Private
{

/** Sentinel values stored in the layer map to detect in-progress DFS nodes (cycle guard). */
static constexpr int32 InProgress = -1;

// Size-estimation constants — tunable after checking real TagGroup nodes on screen.
static constexpr float TitleCharWidth      = 7.0f;
static constexpr float TitleWidthPadding   = 60.0f;
static constexpr float NodeMinWidth        = 160.0f;
static constexpr float NodeMaxWidth        = 400.0f;
static constexpr float NodeBaseHeight      = 64.0f;
static constexpr float TagGroupMemberPitch = 18.0f;

/**
 * Recursively compute the longest-path layer for Node.
 *
 * LayerMap  — persistent result cache; absent = not yet visited.
 * InStack   — nodes currently on the DFS call stack for cycle detection.
 *
 * Returns the resolved layer (>= 0).  Back-edges (InStack hit) contribute 0 so
 * cycles cannot cause infinite recursion or bogus deep layers.
 */
static int32 ComputeLayer(
	UArcRecipeGraphNode* Node,
	TMap<UArcRecipeGraphNode*, int32>& LayerMap,
	TSet<UArcRecipeGraphNode*>& InStack)
{
	// Already resolved — return cached result.
	if (const int32* Cached = LayerMap.Find(Node))
	{
		return *Cached;
	}

	// Mark as in-progress before recursing so back-edges return 0.
	InStack.Add(Node);

	UEdGraphPin* InputPin = Node->GetInputPin();

	int32 MaxProducerLayer = -1;

	if (InputPin != nullptr)
	{
		for (UEdGraphPin* LinkedPin : InputPin->LinkedTo)
		{
			if (LinkedPin == nullptr)
			{
				continue;
			}

			UArcRecipeGraphNode* Producer = Cast<UArcRecipeGraphNode>(LinkedPin->GetOwningNodeUnchecked());
			if (Producer == nullptr)
			{
				continue;
			}

			// Back-edge — skip to break the cycle.
			if (InStack.Contains(Producer))
			{
				continue;
			}

			int32 ProducerLayer = ComputeLayer(Producer, LayerMap, InStack);
			if (ProducerLayer > MaxProducerLayer)
			{
				MaxProducerLayer = ProducerLayer;
			}
		}
	}

	InStack.Remove(Node);

	int32 Layer = (MaxProducerLayer < 0) ? 0 : (MaxProducerLayer + 1);
	LayerMap.Add(Node, Layer);
	return Layer;
}

/**
 * Return the estimated or measured size of a graph node in Slate units.
 *
 * When the node's widget is live and reports a non-zero desired size,
 * use that measurement (true when arranging via the combo box on a live graph).
 * Otherwise fall back to title-length + node-type estimation.
 */
static FVector2D GetNodeSize(const UArcRecipeGraphNode& Node)
{
	TSharedPtr<SGraphNode> Widget = Node.DEPRECATED_NodeWidget.Pin();
	if (Widget.IsValid())
	{
		FVector2D DesiredSize = Widget->GetDesiredSize();
		if (DesiredSize.X > 0.0f && DesiredSize.Y > 0.0f)
		{
			return DesiredSize;
		}
	}

	// Estimate from title and node type.
	const float TitleLen = static_cast<float>(Node.DisplayTitle.ToString().Len());
	const float Width = FMath::Clamp(
		TitleLen * TitleCharWidth + TitleWidthPadding,
		NodeMinWidth,
		NodeMaxWidth);

	float Height = NodeBaseHeight;
	if (Node.NodeType == EArcRecipeGraphNodeType::TagGroup)
	{
		Height += static_cast<float>(Node.GroupMembers.Num()) * TagGroupMemberPitch;
	}

	return FVector2D(Width, Height);
}

} // namespace ArcRecipeGraphLayout_Private

// ----------------------------------------------------------------------------
// FArcRecipeGraphLayout — public static methods
// ----------------------------------------------------------------------------

FArcRecipeLayoutModel FArcRecipeGraphLayout::BuildModel(
	UArcRecipeGraph& Graph,
	TArray<UArcRecipeGraphNode*>& OutSourceNodes)
{
	// Collect nodes in stable Graph.Nodes array order.
	OutSourceNodes.Reset();
	OutSourceNodes.Reserve(Graph.Nodes.Num());
	for (UEdGraphNode* RawNode : Graph.Nodes)
	{
		UArcRecipeGraphNode* Node = Cast<UArcRecipeGraphNode>(RawNode);
		if (Node != nullptr)
		{
			OutSourceNodes.Add(Node);
		}
	}

	FArcRecipeLayoutModel Model;

	if (OutSourceNodes.Num() == 0)
	{
		return Model;
	}

	const int32 NodeCount = OutSourceNodes.Num();
	Model.Nodes.SetNum(NodeCount);
	Model.NumRealNodes = NodeCount;

	// Build a reverse lookup: UArcRecipeGraphNode* -> model index.
	TMap<UArcRecipeGraphNode*, int32> NodeToIndex;
	NodeToIndex.Reserve(NodeCount);
	for (int32 i = 0; i < NodeCount; ++i)
	{
		NodeToIndex.Add(OutSourceNodes[i], i);
	}

	// --- Pass 1: compute layers via memoised DFS. ---
	TMap<UArcRecipeGraphNode*, int32> LayerMap;
	LayerMap.Reserve(NodeCount);
	TSet<UArcRecipeGraphNode*> InStack;
	InStack.Reserve(NodeCount);

	for (UArcRecipeGraphNode* Node : OutSourceNodes)
	{
		ArcRecipeGraphLayout_Private::ComputeLayer(Node, LayerMap, InStack);
	}

	// --- Pass 2: populate model nodes with layer + size. ---
	int32 MaxLayer = 0;
	for (int32 i = 0; i < NodeCount; ++i)
	{
		UArcRecipeGraphNode* SourceNode = OutSourceNodes[i];
		const int32 Layer = LayerMap.FindRef(SourceNode);
		Model.Nodes[i].Layer = Layer;
		Model.Nodes[i].Size  = ArcRecipeGraphLayout_Private::GetNodeSize(*SourceNode);
		if (Layer > MaxLayer)
		{
			MaxLayer = Layer;
		}
	}

	// --- Pass 3: build Layers arrays (insertion order). ---
	Model.Layers.SetNum(MaxLayer + 1);
	for (int32 i = 0; i < NodeCount; ++i)
	{
		Model.Layers[Model.Nodes[i].Layer].Add(i);
	}

	// --- Pass 4: build Edges from output pins (producer -> consumer). ---
	TSet<TPair<int32, int32>> SeenEdges;
	for (int32 ProducerIdx = 0; ProducerIdx < NodeCount; ++ProducerIdx)
	{
		UArcRecipeGraphNode* ProducerNode = OutSourceNodes[ProducerIdx];
		UEdGraphPin* OutputPin = ProducerNode->GetOutputPin();
		if (OutputPin == nullptr)
		{
			continue;
		}

		for (UEdGraphPin* LinkedPin : OutputPin->LinkedTo)
		{
			if (LinkedPin == nullptr)
			{
				continue;
			}

			UArcRecipeGraphNode* ConsumerNode = Cast<UArcRecipeGraphNode>(
				LinkedPin->GetOwningNodeUnchecked());
			if (ConsumerNode == nullptr)
			{
				continue;
			}

			const int32* ConsumerIdxPtr = NodeToIndex.Find(ConsumerNode);
			if (ConsumerIdxPtr == nullptr)
			{
				continue;
			}

			const int32 ConsumerIdx = *ConsumerIdxPtr;
			TPair<int32, int32> EdgeKey(ProducerIdx, ConsumerIdx);
			if (SeenEdges.Contains(EdgeKey))
			{
				continue;
			}
			SeenEdges.Add(EdgeKey);

			FArcRecipeLayoutEdge Edge;
			Edge.From = ProducerIdx;
			Edge.To   = ConsumerIdx;
			Model.Edges.Add(Edge);
		}
	}

	// --- Pass 5: derive InNodes / OutNodes from Edges. ---
	RebuildAdjacency(Model);

	return Model;
}

void FArcRecipeGraphLayout::RebuildAdjacency(FArcRecipeLayoutModel& Model)
{
	for (FArcRecipeLayoutNode& Node : Model.Nodes)
	{
		Node.InNodes.Reset();
		Node.OutNodes.Reset();
	}

	for (const FArcRecipeLayoutEdge& Edge : Model.Edges)
	{
		if (Edge.From == INDEX_NONE || Edge.To == INDEX_NONE)
		{
			continue;
		}
		if (!Model.Nodes.IsValidIndex(Edge.From) || !Model.Nodes.IsValidIndex(Edge.To))
		{
			continue;
		}
		Model.Nodes[Edge.From].OutNodes.Add(Edge.To);
		Model.Nodes[Edge.To].InNodes.Add(Edge.From);
	}
}

void FArcRecipeGraphLayout::WritePositions(
	const FArcRecipeLayoutModel& Model,
	const TArray<UArcRecipeGraphNode*>& SourceNodes)
{
	const int32 RealCount = FMath::Min(Model.NumRealNodes, SourceNodes.Num());
	for (int32 i = 0; i < RealCount; ++i)
	{
		SourceNodes[i]->NodePosX = FMath::RoundToInt(Model.Nodes[i].Pos.X);
		SourceNodes[i]->NodePosY = FMath::RoundToInt(Model.Nodes[i].Pos.Y);
	}
}

void FArcRecipeGraphLayout::Apply(UArcRecipeGraph& Graph, EArcRecipeGraphLayoutMode Mode)
{
	switch (Mode)
	{
		case EArcRecipeGraphLayoutMode::MinCrossings:
		{
			TArray<UArcRecipeGraphNode*> SourceNodes;
			FArcRecipeLayoutModel Model = BuildModel(Graph, SourceNodes);
			ArcRecipeGraphLayoutLayered::MinimizeCrossings(Model);
			ArcRecipeGraphLayoutLayered::AssignCoordinates(Model);
			WritePositions(Model, SourceNodes);
			break;
		}

		case EArcRecipeGraphLayoutMode::Straightened:
		{
			TArray<UArcRecipeGraphNode*> SourceNodes;
			FArcRecipeLayoutModel Model = BuildModel(Graph, SourceNodes);
			ArcRecipeGraphLayoutLayered::MinimizeCrossings(Model);
			ArcRecipeGraphLayoutLayered::AssignCoordinates(Model);
			ArcRecipeGraphLayoutLayered::StraightenY(Model);
			WritePositions(Model, SourceNodes);
			break;
		}

		case EArcRecipeGraphLayoutMode::Sugiyama:
		{
			TArray<UArcRecipeGraphNode*> SourceNodes;
			FArcRecipeLayoutModel Model = BuildModel(Graph, SourceNodes);
			ArcRecipeGraphLayoutSugiyama::ApplySugiyama(Model);
			WritePositions(Model, SourceNodes);
			break;
		}

		case EArcRecipeGraphLayoutMode::ForceDirected:
		{
			TArray<UArcRecipeGraphNode*> SourceNodes;
			FArcRecipeLayoutModel Model = BuildModel(Graph, SourceNodes);
			ArcRecipeGraphLayoutLayered::MinimizeCrossings(Model);
			ArcRecipeGraphLayoutLayered::AssignCoordinates(Model);
			ArcRecipeGraphLayoutForce::ApplyForceDirected(Model);
			WritePositions(Model, SourceNodes);
			break;
		}

		case EArcRecipeGraphLayoutMode::Compact:
		default:
		{
			// --- Compact (legacy) mode ---
			// Collect all recipe graph nodes in stable array order.
			TArray<UArcRecipeGraphNode*> Nodes;
			Nodes.Reserve(Graph.Nodes.Num());
			for (UEdGraphNode* RawNode : Graph.Nodes)
			{
				UArcRecipeGraphNode* Node = Cast<UArcRecipeGraphNode>(RawNode);
				if (Node != nullptr)
				{
					Nodes.Add(Node);
				}
			}

			if (Nodes.Num() == 0)
			{
				return;
			}

			// Pass 1: compute layer for every node via memoised DFS.
			TMap<UArcRecipeGraphNode*, int32> LayerMap;
			LayerMap.Reserve(Nodes.Num());
			TSet<UArcRecipeGraphNode*> InStack;
			InStack.Reserve(Nodes.Num());

			for (UArcRecipeGraphNode* Node : Nodes)
			{
				ArcRecipeGraphLayout_Private::ComputeLayer(Node, LayerMap, InStack);
			}

			// Pass 2: assign positions — fixed 320x120 grid.
			TMap<int32, int32> LayerCounter;

			static constexpr int32 LayerSpacingX = 320;
			static constexpr int32 NodeSpacingY  = 120;

			for (UArcRecipeGraphNode* Node : Nodes)
			{
				const int32 Layer = LayerMap.FindRef(Node);

				int32& CountInLayer = LayerCounter.FindOrAdd(Layer);
				const int32 IndexInLayer = CountInLayer;
				++CountInLayer;

				Node->NodePosX = Layer * LayerSpacingX;
				Node->NodePosY = IndexInLayer * NodeSpacingY;
			}

			break;
		}
	}
}
