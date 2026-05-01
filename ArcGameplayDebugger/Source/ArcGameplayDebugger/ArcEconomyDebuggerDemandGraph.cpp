// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyDebugger.h"

#include "imgui.h"
#include "Mass/ArcDemandGraph.h"
#include "Mass/ArcEconomyFragments.h"
#include "MassEntitySubsystem.h"
#include "ArcSettlementSubsystem.h"
#include "Items/ArcItemDefinition.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

namespace Arcx::GameplayDebugger::Economy
{
	extern UWorld* GetDebugWorld();
	extern FMassEntityManager* GetEntityManager();
	extern UArcSettlementSubsystem* GetSettlementSubsystem();
} // namespace Arcx::GameplayDebugger::Economy

namespace
{
	constexpr float NodeWidth = 160.0f;
	constexpr float NodeHeight = 55.0f;
	constexpr float LayerSpacingX = 200.0f;
	constexpr float NodeSpacingY = 70.0f;
	constexpr float CanvasOffsetX = 40.0f;
	constexpr float CanvasOffsetY = 20.0f;
	constexpr float ArrowSize = 8.0f;

	ImU32 GetNodeColor(ArcDemandGraph::ENodeStatus Status)
	{
		switch (Status)
		{
		case ArcDemandGraph::ENodeStatus::Flowing:       return IM_COL32(80, 180, 80, 255);
		case ArcDemandGraph::ENodeStatus::Backpressured: return IM_COL32(220, 190, 40, 255);
		case ArcDemandGraph::ENodeStatus::Unstaffed:     return IM_COL32(220, 190, 40, 255);
		case ArcDemandGraph::ENodeStatus::UnmetDemand:   return IM_COL32(210, 70, 70, 255);
		case ArcDemandGraph::ENodeStatus::Potential:     return IM_COL32(70, 150, 200, 255);
		case ArcDemandGraph::ENodeStatus::Idle:
		default:                                          return IM_COL32(140, 140, 140, 255);
		}
	}

	const char* GetNodeStatusName(ArcDemandGraph::ENodeStatus Status)
	{
		switch (Status)
		{
		case ArcDemandGraph::ENodeStatus::Flowing:       return "Flowing";
		case ArcDemandGraph::ENodeStatus::Backpressured: return "Backpressured";
		case ArcDemandGraph::ENodeStatus::Unstaffed:     return "Unstaffed";
		case ArcDemandGraph::ENodeStatus::UnmetDemand:   return "UnmetDemand";
		case ArcDemandGraph::ENodeStatus::Potential:     return "Potential";
		case ArcDemandGraph::ENodeStatus::Idle:
		default:                                          return "Idle";
		}
	}

	const char* GetNodeTypeName(ArcDemandGraph::ENodeType Type)
	{
		switch (Type)
		{
		case ArcDemandGraph::ENodeType::Consumer:   return "Consumer";
		case ArcDemandGraph::ENodeType::Producer:   return "Producer";
		case ArcDemandGraph::ENodeType::Settlement: return "Settlement";
		default:                                     return "Unknown";
		}
	}
} // anonymous namespace

void FArcEconomyDebugger::RefreshDemandGraph()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		CachedDemandGraph.Reset();
		NodePositions.Reset();
		return;
	}

	FMassEntityManager* Manager = Arcx::GameplayDebugger::Economy::GetEntityManager();
	UArcSettlementSubsystem* SettlementSub = Arcx::GameplayDebugger::Economy::GetSettlementSubsystem();
	if (!Manager || !SettlementSub)
	{
		return;
	}

	const FSettlementEntry& Settlement = CachedSettlements[SelectedSettlementIdx];
	if (!Manager->IsEntityActive(Settlement.Entity))
	{
		return;
	}

	const TArray<FMassEntityHandle>& BuildingHandles = SettlementSub->GetBuildingsForSettlement(Settlement.Entity);

	FStructView MarketView = Manager->GetFragmentDataStruct(
		Settlement.Entity, FArcSettlementMarketFragment::StaticStruct());
	if (!MarketView.IsValid())
	{
		return;
	}

	const FArcSettlementMarketFragment& Market = MarketView.Get<FArcSettlementMarketFragment>();
	CachedDemandGraph.Rebuild(*Manager, Settlement.Entity, BuildingHandles, Market);

	// Compute layered layout positions based on chain depth.
	// Edges go Consumer→Producer, so we BFS from rightmost (leaf consumers
	// with no outgoing edges as consumers) leftward through producers.
	// Layer 0 = settlement, higher layers = deeper into the supply chain.
	// Final consumers are on the right, base producers (no inputs) on the left.
	NodePositions.Reset();

	const int32 NumNodes = CachedDemandGraph.Nodes.Num();
	TArray<int32> NodeLayer;
	NodeLayer.SetNumZeroed(NumNodes);

	// Build adjacency: for each producer node, find which consumer nodes it feeds
	// (consumer→producer edges mean producer supplies consumer).
	// We want to walk from consumers toward producers to assign depth.
	// Assign layers by walking edges: consumers get max layer, producers get layer - 1.

	// Step 1: Find max chain depth per node via reverse BFS.
	// Start by finding all "leaf" consumer nodes (consumers that are NOT also
	// a producer's input — i.e., the final demand endpoints like taverns).
	TSet<int32> NodesAsConsumerInEdge;
	TSet<int32> NodesAsProducerInEdge;
	for (const ArcDemandGraph::FEdge& Edge : CachedDemandGraph.Edges)
	{
		NodesAsConsumerInEdge.Add(Edge.ConsumerIdx);
		NodesAsProducerInEdge.Add(Edge.ProducerIdx);
	}

	// Find which producer nodes also have consumer nodes on the same building entity
	// (meaning they are intermediate — they produce something AND need inputs).
	// Build a map: ProducerNodeIdx → ConsumerNodeIndices (same entity, meaning its inputs)
	TMap<int32, TArray<int32>> ProducerToInputConsumers;
	for (int32 Idx = 0; Idx < NumNodes; ++Idx)
	{
		const ArcDemandGraph::FNode& Node = CachedDemandGraph.Nodes[Idx];
		if (Node.Type != ArcDemandGraph::ENodeType::Consumer)
		{
			continue;
		}
		// Find which producer this consumer feeds (via edges)
		// Actually, the edges go consumer→producer, meaning "consumer needs what producer makes".
		// A consumer node that is an INPUT of a producer shares the same Entity as that producer.
		// Find the producer node for the same entity.
		for (int32 ProdIdx = 0; ProdIdx < NumNodes; ++ProdIdx)
		{
			const ArcDemandGraph::FNode& ProdNode = CachedDemandGraph.Nodes[ProdIdx];
			if (ProdNode.Type != ArcDemandGraph::ENodeType::Producer)
			{
				continue;
			}
			if (ProdNode.Entity == Node.Entity)
			{
				ProducerToInputConsumers.FindOrAdd(ProdIdx).AddUnique(Idx);
			}
		}
	}

	// Assign layers by traversing from the edges.
	// Consumer → Producer edges: the consumer is one layer to the right of the producer.
	// We do a forward pass: start from producers with no inputs (layer 1),
	// then their output consumers are layer 2, etc.

	// Actually, simpler approach: use edges to compute depth.
	// Settlement = layer 0. Producers directly linked to settlement-level consumers
	// are at a depth computed by how many producer→consumer→producer hops exist.

	// Simplest correct approach: BFS from edges.
	// For each edge (consumer→producer), the producer is "upstream" of the consumer.
	// Assign depth by: producers with no input consumers = depth 1.
	// Consumers that link to depth-1 producers = depth 2.
	// Producers that have input consumers at depth 2 = depth 3. Etc.

	// Initialize: settlement node = 0, all others = -1
	for (int32 Idx = 0; Idx < NumNodes; ++Idx)
	{
		NodeLayer[Idx] = -1;
	}
	if (NumNodes > 0)
	{
		NodeLayer[0] = 0; // settlement
	}

	// Find base producers (producers that have no input consumer nodes on their entity)
	TArray<int32> BFSQueue;
	for (int32 Idx = 0; Idx < NumNodes; ++Idx)
	{
		const ArcDemandGraph::FNode& Node = CachedDemandGraph.Nodes[Idx];
		if (Node.Type != ArcDemandGraph::ENodeType::Producer)
		{
			continue;
		}
		const TArray<int32>* InputConsumers = ProducerToInputConsumers.Find(Idx);
		if (!InputConsumers || InputConsumers->Num() == 0)
		{
			// Base producer — no inputs needed (e.g., Iron Mine)
			NodeLayer[Idx] = 1;
			BFSQueue.Add(Idx);
		}
	}

	// BFS forward: from each producer, find consumer nodes that link TO it (via edges),
	// assign them layer = producer layer + 1.
	// Then find the producer that has those consumers as inputs, assign layer = consumer layer + 1.
	int32 QueueHead = 0;
	while (QueueHead < BFSQueue.Num())
	{
		const int32 CurrentIdx = BFSQueue[QueueHead++];
		const int32 CurrentLayer = NodeLayer[CurrentIdx];
		const ArcDemandGraph::FNode& CurrentNode = CachedDemandGraph.Nodes[CurrentIdx];

		if (CurrentNode.Type == ArcDemandGraph::ENodeType::Producer)
		{
			// Find consumer nodes that link to this producer (consumer→producer edges)
			for (const ArcDemandGraph::FEdge& Edge : CachedDemandGraph.Edges)
			{
				if (Edge.ProducerIdx != CurrentIdx)
				{
					continue;
				}
				const int32 ConsumerIdx = Edge.ConsumerIdx;
				const int32 NewLayer = CurrentLayer + 1;
				if (NodeLayer[ConsumerIdx] < NewLayer)
				{
					NodeLayer[ConsumerIdx] = NewLayer;
					BFSQueue.Add(ConsumerIdx);
				}
			}
		}
		else if (CurrentNode.Type == ArcDemandGraph::ENodeType::Consumer)
		{
			// This consumer is an input for a producer on the same entity.
			// Find that producer and set its layer = consumer layer + 1.
			for (const TPair<int32, TArray<int32>>& Pair : ProducerToInputConsumers)
			{
				if (Pair.Value.Contains(CurrentIdx))
				{
					const int32 ProdIdx = Pair.Key;
					const int32 NewLayer = CurrentLayer + 1;
					if (NodeLayer[ProdIdx] < NewLayer)
					{
						NodeLayer[ProdIdx] = NewLayer;
						BFSQueue.Add(ProdIdx);
					}
				}
			}
		}
	}

	// Any unassigned nodes get a fallback layer
	int32 MaxLayer = 1;
	for (int32 Idx = 0; Idx < NumNodes; ++Idx)
	{
		if (NodeLayer[Idx] > MaxLayer)
		{
			MaxLayer = NodeLayer[Idx];
		}
	}
	for (int32 Idx = 0; Idx < NumNodes; ++Idx)
	{
		if (NodeLayer[Idx] < 0)
		{
			NodeLayer[Idx] = MaxLayer + 1;
		}
	}

	// Count nodes per layer for vertical stacking
	TMap<int32, int32> LayerCounters;
	for (int32 Idx = 0; Idx < NumNodes; ++Idx)
	{
		const int32 Layer = NodeLayer[Idx];
		int32& Counter = LayerCounters.FindOrAdd(Layer);
		float PosX = Layer * LayerSpacingX + CanvasOffsetX;
		float PosY = Counter * NodeSpacingY + CanvasOffsetY;
		NodePositions.Add(Idx, FVector2D(PosX, PosY));
		++Counter;
	}
#endif
}

void FArcEconomyDebugger::DrawDemandGraphTab()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedSettlementIdx == INDEX_NONE || !CachedSettlements.IsValidIndex(SelectedSettlementIdx))
	{
		ImGui::TextDisabled("Select a settlement to view its demand graph");
		return;
	}

	RefreshDemandGraph();

	if (CachedDemandGraph.Nodes.Num() == 0)
	{
		ImGui::TextDisabled("No demand graph data available");
		return;
	}

	ImVec2 CanvasSize = ImGui::GetContentRegionAvail();
	if (CanvasSize.x < 50.0f || CanvasSize.y < 50.0f)
	{
		return;
	}

	ImVec2 CanvasOrigin = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton("DemandGraphCanvas", CanvasSize);

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	ImVec2 MousePos = ImGui::GetMousePos();

	// Background
	DrawList->AddRectFilled(
		CanvasOrigin,
		ImVec2(CanvasOrigin.x + CanvasSize.x, CanvasOrigin.y + CanvasSize.y),
		IM_COL32(30, 30, 30, 255));

	// Layer 0 label (settlement)
	DrawList->AddText(
		ImVec2(CanvasOrigin.x + CanvasOffsetX, CanvasOrigin.y + 2.0f),
		IM_COL32(200, 200, 200, 180), "Settlement");

	// Draw edges first (behind nodes)
	const ImU32 EdgeColor = IM_COL32(255, 255, 255, 180);
	for (const ArcDemandGraph::FEdge& Edge : CachedDemandGraph.Edges)
	{
		const FVector2D* ConsumerPos = NodePositions.Find(Edge.ConsumerIdx);
		const FVector2D* ProducerPos = NodePositions.Find(Edge.ProducerIdx);
		if (!ConsumerPos || !ProducerPos)
		{
			continue;
		}

		// Consumer left-center to Producer right-center
		ImVec2 Start(
			CanvasOrigin.x + static_cast<float>(ConsumerPos->X),
			CanvasOrigin.y + static_cast<float>(ConsumerPos->Y) + NodeHeight * 0.5f);
		ImVec2 End(
			CanvasOrigin.x + static_cast<float>(ProducerPos->X) + NodeWidth,
			CanvasOrigin.y + static_cast<float>(ProducerPos->Y) + NodeHeight * 0.5f);

		DrawList->AddLine(Start, End, EdgeColor, 1.5f);

		// Arrowhead at the producer end (right side)
		ImVec2 Dir(End.x - Start.x, End.y - Start.y);
		float Len = FMath::Sqrt(Dir.x * Dir.x + Dir.y * Dir.y);
		if (Len > 0.0f)
		{
			Dir.x /= Len;
			Dir.y /= Len;
			ImVec2 Perp(-Dir.y, Dir.x);
			ImVec2 ArrowTip = End;
			ImVec2 ArrowLeft(ArrowTip.x - Dir.x * ArrowSize + Perp.x * ArrowSize * 0.5f,
				ArrowTip.y - Dir.y * ArrowSize + Perp.y * ArrowSize * 0.5f);
			ImVec2 ArrowRight(ArrowTip.x - Dir.x * ArrowSize - Perp.x * ArrowSize * 0.5f,
				ArrowTip.y - Dir.y * ArrowSize - Perp.y * ArrowSize * 0.5f);
			DrawList->AddTriangleFilled(ArrowTip, ArrowLeft, ArrowRight, EdgeColor);
		}

		// Demand label at midpoint
		ImVec2 MidPoint((Start.x + End.x) * 0.5f, (Start.y + End.y) * 0.5f - 10.0f);
		char DemandLabel[32];
		FCStringAnsi::Snprintf(DemandLabel, sizeof(DemandLabel), "%d", Edge.DemandQuantity);
		DrawList->AddText(MidPoint, IM_COL32(255, 255, 200, 220), DemandLabel);
	}

	// Draw internal building links: producer → its input consumers (same entity)
	// These show the "Smelter produces bars" → "Smelter needs ore" connection
	const ImU32 InternalEdgeColor = IM_COL32(180, 180, 255, 140);
	for (int32 ProdIdx = 0; ProdIdx < CachedDemandGraph.Nodes.Num(); ++ProdIdx)
	{
		const ArcDemandGraph::FNode& ProdNode = CachedDemandGraph.Nodes[ProdIdx];
		if (ProdNode.Type != ArcDemandGraph::ENodeType::Producer)
		{
			continue;
		}
		const FVector2D* ProdPos = NodePositions.Find(ProdIdx);
		if (!ProdPos)
		{
			continue;
		}

		for (int32 ConsIdx = 0; ConsIdx < CachedDemandGraph.Nodes.Num(); ++ConsIdx)
		{
			const ArcDemandGraph::FNode& ConsNode = CachedDemandGraph.Nodes[ConsIdx];
			if (ConsNode.Type != ArcDemandGraph::ENodeType::Consumer)
			{
				continue;
			}
			if (ConsNode.Entity != ProdNode.Entity)
			{
				continue;
			}
			const FVector2D* ConsPos = NodePositions.Find(ConsIdx);
			if (!ConsPos)
			{
				continue;
			}

			// Draw from producer left-center to consumer right-center
			ImVec2 Start(
				CanvasOrigin.x + static_cast<float>(ProdPos->X),
				CanvasOrigin.y + static_cast<float>(ProdPos->Y) + NodeHeight * 0.5f);
			ImVec2 End(
				CanvasOrigin.x + static_cast<float>(ConsPos->X) + NodeWidth,
				CanvasOrigin.y + static_cast<float>(ConsPos->Y) + NodeHeight * 0.5f);

			DrawList->AddLine(Start, End, InternalEdgeColor, 1.5f);

			// Arrowhead
			ImVec2 Dir(End.x - Start.x, End.y - Start.y);
			float Len = FMath::Sqrt(Dir.x * Dir.x + Dir.y * Dir.y);
			if (Len > 0.0f)
			{
				Dir.x /= Len;
				Dir.y /= Len;
				ImVec2 Perp(-Dir.y, Dir.x);
				ImVec2 ArrowTip = End;
				ImVec2 ArrowLeft(ArrowTip.x - Dir.x * ArrowSize + Perp.x * ArrowSize * 0.5f,
					ArrowTip.y - Dir.y * ArrowSize + Perp.y * ArrowSize * 0.5f);
				ImVec2 ArrowRight(ArrowTip.x - Dir.x * ArrowSize - Perp.x * ArrowSize * 0.5f,
					ArrowTip.y - Dir.y * ArrowSize - Perp.y * ArrowSize * 0.5f);
				DrawList->AddTriangleFilled(ArrowTip, ArrowLeft, ArrowRight, InternalEdgeColor);
			}
		}
	}

	// Draw nodes
	HoveredNodeIdx = INDEX_NONE;

	for (int32 Idx = 0; Idx < CachedDemandGraph.Nodes.Num(); ++Idx)
	{
		const ArcDemandGraph::FNode& Node = CachedDemandGraph.Nodes[Idx];
		const FVector2D* Pos = NodePositions.Find(Idx);
		if (!Pos)
		{
			continue;
		}

		ImVec2 RectMin(CanvasOrigin.x + static_cast<float>(Pos->X),
			CanvasOrigin.y + static_cast<float>(Pos->Y));
		ImVec2 RectMax(RectMin.x + NodeWidth, RectMin.y + NodeHeight);

		ImU32 FillColor = GetNodeColor(Node.Status);
		DrawList->AddRectFilled(RectMin, RectMax, FillColor, 4.0f);

		// Hover detection
		bool bHovered = (MousePos.x >= RectMin.x && MousePos.x <= RectMax.x
			&& MousePos.y >= RectMin.y && MousePos.y <= RectMax.y);

		if (bHovered)
		{
			HoveredNodeIdx = Idx;
			DrawList->AddRect(RectMin, RectMax, IM_COL32(255, 255, 255, 255), 4.0f, 0, 2.5f);
		}
		else if (Node.Status == ArcDemandGraph::ENodeStatus::Potential)
		{
			// Dashed border for potential nodes
			const ImU32 DashColor = IM_COL32(200, 200, 200, 180);
			const float DashLen = 6.0f;
			const float GapLen = 4.0f;

			for (float X = RectMin.x; X < RectMax.x; X += DashLen + GapLen)
			{
				float EndX = FMath::Min(X + DashLen, RectMax.x);
				DrawList->AddLine(ImVec2(X, RectMin.y), ImVec2(EndX, RectMin.y), DashColor, 1.5f);
			}
			for (float X = RectMin.x; X < RectMax.x; X += DashLen + GapLen)
			{
				float EndX = FMath::Min(X + DashLen, RectMax.x);
				DrawList->AddLine(ImVec2(X, RectMax.y), ImVec2(EndX, RectMax.y), DashColor, 1.5f);
			}
			for (float Y = RectMin.y; Y < RectMax.y; Y += DashLen + GapLen)
			{
				float EndY = FMath::Min(Y + DashLen, RectMax.y);
				DrawList->AddLine(ImVec2(RectMin.x, Y), ImVec2(RectMin.x, EndY), DashColor, 1.5f);
			}
			for (float Y = RectMin.y; Y < RectMax.y; Y += DashLen + GapLen)
			{
				float EndY = FMath::Min(Y + DashLen, RectMax.y);
				DrawList->AddLine(ImVec2(RectMax.x, Y), ImVec2(RectMax.x, EndY), DashColor, 1.5f);
			}
		}
		else
		{
			DrawList->AddRect(RectMin, RectMax, IM_COL32(200, 200, 200, 120), 4.0f);
		}

		// Text line 1: building name
		FString BuildingNameStr = Node.BuildingName.ToString();
		DrawList->AddText(
			ImVec2(RectMin.x + 4.0f, RectMin.y + 4.0f),
			IM_COL32(255, 255, 255, 255),
			TCHAR_TO_ANSI(*BuildingNameStr));

		// Text line 2: item info
		char InfoLine[128];
		switch (Node.Type)
		{
		case ArcDemandGraph::ENodeType::Consumer:
		{
			FString ItemName = Node.ItemDef ? Node.ItemDef->GetName() : TEXT("?");
			FCStringAnsi::Snprintf(InfoLine, sizeof(InfoLine), "%s -%d",
				TCHAR_TO_ANSI(*ItemName), Node.DemandQuantity);
			break;
		}
		case ArcDemandGraph::ENodeType::Producer:
		{
			FString ItemName = Node.ItemDef ? Node.ItemDef->GetName() : TEXT("?");
			FCStringAnsi::Snprintf(InfoLine, sizeof(InfoLine), "%s [%d/%d]",
				TCHAR_TO_ANSI(*ItemName), Node.CurrentStock, Node.StorageCap);
			break;
		}
		case ArcDemandGraph::ENodeType::Settlement:
		{
			FCStringAnsi::Snprintf(InfoLine, sizeof(InfoLine), "Settlement [%d/%d]",
				Node.CurrentStock, Node.StorageCap);
			break;
		}
		}

		DrawList->AddText(
			ImVec2(RectMin.x + 4.0f, RectMin.y + 20.0f),
			IM_COL32(220, 220, 220, 220),
			InfoLine);
	}

	// Tooltip for hovered node
	if (HoveredNodeIdx != INDEX_NONE && CachedDemandGraph.Nodes.IsValidIndex(HoveredNodeIdx))
	{
		const ArcDemandGraph::FNode& Node = CachedDemandGraph.Nodes[HoveredNodeIdx];
		ImGui::BeginTooltip();
		ImGui::Text("Building: %s", TCHAR_TO_ANSI(*Node.BuildingName.ToString()));
		ImGui::Text("Type: %s", GetNodeTypeName(Node.Type));
		ImGui::Text("Status: %s", GetNodeStatusName(Node.Status));
		if (Node.ItemDef)
		{
			ImGui::Text("Item: %s", TCHAR_TO_ANSI(*Node.ItemDef->GetName()));
		}
		ImGui::Text("Demand: %d", Node.DemandQuantity);
		ImGui::Text("Stock: %d / %d", Node.CurrentStock, Node.StorageCap);
		ImGui::Text("Staffed: %s", Node.bStaffed ? "Yes" : "No");
		ImGui::Text("Backpressured: %s", Node.bBackpressured ? "Yes" : "No");
		ImGui::Text("Entity: %d", Node.Entity.IsValid() ? Node.Entity.Index : -1);
		if (Node.Status == ArcDemandGraph::ENodeStatus::Potential)
		{
			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.3f, 0.6f, 0.8f, 1.0f), "Recipe not yet activated");
			ImGui::Text("Will be activated by governor");
		}
		ImGui::EndTooltip();
	}
#endif
}
