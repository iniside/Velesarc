// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/Editor/ArcHeightmapGraphBridge.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "StructUtils/InstancedStruct.h"

#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "TerrainTool/Graph/ArcHeightmapNodes.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraph.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraphNode.h"

namespace ArcHeightmapGraphBridgeInternal
{
	/** Resolves a linked pin's owning mirror node's NodeId, or an invalid FGuid. */
	FGuid LinkedNodeId(const UEdGraphPin& InputPin)
	{
		for (const UEdGraphPin* Linked : InputPin.LinkedTo)
		{
			if (Linked == nullptr)
			{
				continue;
			}
			const UArcHeightmapEdGraphNode* Source = Cast<UArcHeightmapEdGraphNode>(Linked->GetOwningNodeUnchecked());
			if (Source != nullptr)
			{
				return Source->NodeId;
			}
		}
		return FGuid();
	}
}

namespace ArcHeightmapGraphBridge
{
	void BuildEdGraphFromAsset(UArcHeightmapGraph& Asset, UArcHeightmapEdGraph& EdGraph)
	{
		// Mint pass: assign NodeIds to any id-less node. Only dirty if a mint happened.
		bool bMinted = false;
		for (FInstancedStruct& Entry : Asset.Nodes)
		{
			if (!Entry.IsValid())
			{
				continue;
			}
			FArcHeightmapNode& Node = Entry.GetMutable<FArcHeightmapNode>();
			if (!Node.NodeId.IsValid())
			{
				Node.NodeId = FGuid::NewGuid();
				bMinted = true;
			}
		}
		if (bMinted)
		{
			Asset.Modify();
			Asset.MarkPackageDirty();
		}

		// Pass 1: spawn one mirror node per entry, indexing by NodeId.
		TMap<FGuid, UArcHeightmapEdGraphNode*> ById;
		ById.Reserve(Asset.Nodes.Num());
		for (const FInstancedStruct& Entry : Asset.Nodes)
		{
			if (!Entry.IsValid())
			{
				continue;
			}
			const FArcHeightmapNode& Node = Entry.Get<FArcHeightmapNode>();

			FGraphNodeCreator<UArcHeightmapEdGraphNode> Creator(EdGraph);
			UArcHeightmapEdGraphNode* EdNode = Creator.CreateNode(false);
			EdNode->NodeId = Node.NodeId;

			const FIntPoint Position = Asset.GetNodePosition(Node.NodeId);
			EdNode->NodePosX = Position.X;
			EdNode->NodePosY = Position.Y;

			Creator.Finalize();

			ById.Add(Node.NodeId, EdNode);
		}

		// Pass 2: wire input pins from InputNodeIds; skip dangling GUIDs silently.
		for (const FInstancedStruct& Entry : Asset.Nodes)
		{
			if (!Entry.IsValid())
			{
				continue;
			}
			const FArcHeightmapNode& Node = Entry.Get<FArcHeightmapNode>();
			UArcHeightmapEdGraphNode** const FoundThis = ById.Find(Node.NodeId);
			if (FoundThis == nullptr)
			{
				continue;
			}
			UArcHeightmapEdGraphNode* const ThisNode = *FoundThis;

			for (int32 InputIndex = 0; InputIndex < Node.InputNodeIds.Num(); ++InputIndex)
			{
				const FGuid& SourceId = Node.InputNodeIds[InputIndex];
				if (!SourceId.IsValid())
				{
					continue;
				}
				UArcHeightmapEdGraphNode** const FoundSource = ById.Find(SourceId);
				if (FoundSource == nullptr)
				{
					continue;
				}

				UEdGraphPin* const SourcePin = (*FoundSource)->GetOutputPin();
				UEdGraphPin* const InputPin = ThisNode->GetInputPin(InputIndex);
				if (SourcePin != nullptr && InputPin != nullptr)
				{
					InputPin->MakeLinkTo(SourcePin);
				}
			}
		}
	}

	void WriteAssetFromEdGraph(UArcHeightmapEdGraph& EdGraph, UArcHeightmapGraph& Asset)
	{
		// Move every existing entry out, keyed by NodeId, preserving authored struct data.
		TMap<FGuid, FInstancedStruct> OldByItem;
		OldByItem.Reserve(Asset.Nodes.Num());
		for (FInstancedStruct& Entry : Asset.Nodes)
		{
			if (!Entry.IsValid())
			{
				continue;
			}
			const FGuid NodeId = Entry.Get<FArcHeightmapNode>().NodeId;
			OldByItem.Add(NodeId, MoveTemp(Entry));
		}

		// Rebuild the node list by walking the ed-graph in its current order.
		TArray<FInstancedStruct> NewNodes;
		NewNodes.Reserve(EdGraph.Nodes.Num());
		for (UEdGraphNode* RawNode : EdGraph.Nodes)
		{
			UArcHeightmapEdGraphNode* const EdNode = Cast<UArcHeightmapEdGraphNode>(RawNode);
			if (EdNode == nullptr)
			{
				continue;
			}

			FInstancedStruct* const Found = OldByItem.Find(EdNode->NodeId);
			if (Found == nullptr)
			{
				// Node creation appends to Asset.Nodes first, so this is a desync guard only.
				UE_LOG(LogTemp, Warning,
					TEXT("ArcHeightmapGraphBridge: ed-node %s has no backing asset entry; dropping it."),
					*EdNode->NodeId.ToString());
				continue;
			}

			FInstancedStruct Entry = MoveTemp(*Found);

			// Rewrite InputNodeIds from the live pin links: one slot per input pin.
			FArcHeightmapNode& Node = Entry.GetMutable<FArcHeightmapNode>();
			const int32 InputCount = Node.GetInputCount();
			Node.InputNodeIds.SetNum(InputCount);
			for (int32 InputIndex = 0; InputIndex < InputCount; ++InputIndex)
			{
				const UEdGraphPin* const InputPin = EdNode->GetInputPin(InputIndex);
				// Unlinked slot → invalid FGuid (intentional dangling-ref prune).
				Node.InputNodeIds[InputIndex] = (InputPin != nullptr)
					? ArcHeightmapGraphBridgeInternal::LinkedNodeId(*InputPin)
					: FGuid();
			}

			NewNodes.Add(MoveTemp(Entry));
		}

		Asset.Nodes = MoveTemp(NewNodes);

		// Rebuild positions from surviving ed-nodes (drops orphaned positions).
		Asset.NodePositions.Reset();
		for (UEdGraphNode* RawNode : EdGraph.Nodes)
		{
			UArcHeightmapEdGraphNode* const EdNode = Cast<UArcHeightmapEdGraphNode>(RawNode);
			if (EdNode == nullptr)
			{
				continue;
			}
			Asset.SetNodePosition(EdNode->NodeId, FIntPoint(EdNode->NodePosX, EdNode->NodePosY));
		}

		Asset.Modify();
		Asset.MarkPackageDirty();
	}
}
