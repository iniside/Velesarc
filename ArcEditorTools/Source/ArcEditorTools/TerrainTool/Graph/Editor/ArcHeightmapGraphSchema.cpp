// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/Editor/ArcHeightmapGraphSchema.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Framework/Commands/GenericCommands.h"
#include "ScopedTransaction.h"
#include "StructUtils/InstancedStruct.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "UObject/UObjectIterator.h"

#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "TerrainTool/Graph/ArcHeightmapNodes.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraph.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraphNode.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapGraphConnectionDrawingPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcHeightmapGraphSchema)

#define LOCTEXT_NAMESPACE "ArcHeightmapGraphSchema"

// ---------------------------------------------------------------------------
// File-scoped helpers (unique across the project)
// ---------------------------------------------------------------------------

namespace ArcHeightmapGraphSchemaInternal
{
	/**
	 * Normalises A/B into OutputPin / InputPin by direction. Either may stay
	 * nullptr if both pins share the same direction.
	 */
	static void CategorizePinsByDirection(
		const UEdGraphPin* A,
		const UEdGraphPin* B,
		const UEdGraphPin*& OutInputPin,
		const UEdGraphPin*& OutOutputPin)
	{
		OutInputPin = nullptr;
		OutOutputPin = nullptr;

		if (A != nullptr && A->Direction == EGPD_Input)
		{
			OutInputPin = A;
		}
		else if (A != nullptr && A->Direction == EGPD_Output)
		{
			OutOutputPin = A;
		}

		if (B != nullptr && B->Direction == EGPD_Input)
		{
			OutInputPin = B;
		}
		else if (B != nullptr && B->Direction == EGPD_Output)
		{
			OutOutputPin = B;
		}
	}

	/**
	 * Walks upstream from StartNode (following each node's input pins → LinkedTo →
	 * their owning nodes, recursively) and returns true if TargetNode is reached.
	 * Used to detect a cycle: if the proposed output node already depends on the
	 * proposed input node, completing the link would close a loop.
	 */
	static bool ReachesUpstream(const UEdGraphNode* StartNode, const UEdGraphNode* TargetNode)
	{
		if (StartNode == nullptr || TargetNode == nullptr)
		{
			return false;
		}

		TSet<const UEdGraphNode*> Visited;
		TArray<const UEdGraphNode*> Stack;
		Stack.Add(StartNode);

		while (Stack.Num() > 0)
		{
			const UEdGraphNode* Current = Stack.Pop(EAllowShrinking::No);
			if (Current == nullptr || Visited.Contains(Current))
			{
				continue;
			}
			Visited.Add(Current);

			if (Current == TargetNode)
			{
				return true;
			}

			for (const UEdGraphPin* Pin : Current->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Input)
				{
					continue;
				}
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked == nullptr)
					{
						continue;
					}
					const UEdGraphNode* Upstream = Linked->GetOwningNodeUnchecked();
					if (Upstream != nullptr && !Visited.Contains(Upstream))
					{
						Stack.Add(Upstream);
					}
				}
			}
		}

		return false;
	}

} // namespace ArcHeightmapGraphSchemaInternal

// ---------------------------------------------------------------------------
// FArcHeightmapGraphSchemaAction_NewNode
// ---------------------------------------------------------------------------

UEdGraphNode* FArcHeightmapGraphSchemaAction_NewNode::PerformAction(
	UEdGraph* ParentGraph,
	UEdGraphPin* FromPin,
	const FVector2f& Location,
	bool bSelectNewNode)
{
	UArcHeightmapEdGraph* EdGraph = Cast<UArcHeightmapEdGraph>(ParentGraph);
	if (EdGraph == nullptr || NodeStruct == nullptr)
	{
		return nullptr;
	}

	UArcHeightmapGraph* Asset = EdGraph->SourceAsset.Get();
	if (Asset == nullptr)
	{
		return nullptr;
	}

	const FScopedTransaction Transaction(
		NSLOCTEXT("ArcHeightmapGraph", "AddNode", "Add Heightmap Node"));
	Asset->Modify();
	ParentGraph->Modify();

	const FGuid NewId = FGuid::NewGuid();

	FInstancedStruct NewEntry;
	NewEntry.InitializeAs(NodeStruct);
	NewEntry.GetMutable<FArcHeightmapNode>().NodeId = NewId;
	Asset->Nodes.Add(MoveTemp(NewEntry));

	FGraphNodeCreator<UArcHeightmapEdGraphNode> Creator(*ParentGraph);
	UArcHeightmapEdGraphNode* NewNode = Creator.CreateNode(bSelectNewNode);
	NewNode->NodeId = NewId;
	NewNode->NodePosX = static_cast<int32>(Location.X);
	NewNode->NodePosY = static_cast<int32>(Location.Y);
	Creator.Finalize();

	if (FromPin != nullptr)
	{
		NewNode->AutowireNewNode(FromPin);
	}

	EdGraph->OnGraphEdited.Broadcast();

	return NewNode;
}

// ---------------------------------------------------------------------------
// UArcHeightmapGraphSchema
// ---------------------------------------------------------------------------

const FPinConnectionResponse UArcHeightmapGraphSchema::CanCreateConnection(
	const UEdGraphPin* A,
	const UEdGraphPin* B) const
{
	if (A == nullptr || B == nullptr)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
			LOCTEXT("NullPin", "Invalid pin."));
	}

	if (A->GetOwningNodeUnchecked() == B->GetOwningNodeUnchecked())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
			LOCTEXT("SelfLoop", "Cannot connect a node to itself."));
	}

	const UEdGraphPin* InputPin = nullptr;
	const UEdGraphPin* OutputPin = nullptr;
	ArcHeightmapGraphSchemaInternal::CategorizePinsByDirection(A, B, InputPin, OutputPin);

	if (InputPin == nullptr || OutputPin == nullptr)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
			LOCTEXT("SameDir", "Connect an output to an input."));
	}

	const UEdGraphNode* OutputNode = OutputPin->GetOwningNodeUnchecked();
	const UEdGraphNode* InputNode = InputPin->GetOwningNodeUnchecked();
	if (ArcHeightmapGraphSchemaInternal::ReachesUpstream(OutputNode, InputNode))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
			LOCTEXT("Cycle", "Would create a cycle."));
	}

	if (InputPin->LinkedTo.Num() > 0)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_BREAK_OTHERS_B,
			LOCTEXT("ReplaceInput", "Replace existing input."));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE,
		LOCTEXT("Connect", "Connect height field."));
}

bool UArcHeightmapGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	const bool bMade = Super::TryCreateConnection(A, B);
	if (bMade && A != nullptr && A->GetOwningNode() != nullptr)
	{
		UArcHeightmapEdGraph* EdGraph = Cast<UArcHeightmapEdGraph>(A->GetOwningNode()->GetGraph());
		if (EdGraph != nullptr)
		{
			EdGraph->OnGraphEdited.Broadcast();
		}
	}
	return bMade;
}

void UArcHeightmapGraphSchema::GetGraphContextActions(
	struct FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const UScriptStruct* BaseStruct = FArcHeightmapNode::StaticStruct();
	const FText Category = LOCTEXT("AddNodeCategory", "Add Node");

	for (TObjectIterator<UScriptStruct> StructIt; StructIt; ++StructIt)
	{
		UScriptStruct* Struct = *StructIt;
		if (Struct == nullptr || Struct == BaseStruct)
		{
			continue;
		}
		if (!Struct->IsChildOf(BaseStruct))
		{
			continue;
		}
		if (Struct->HasMetaData(TEXT("ExcludeBaseStruct")))
		{
			continue;
		}

		FString DisplayName = Struct->GetDisplayNameText().ToString();
		DisplayName.RemoveFromStart(TEXT("ArcHeightmapNode_"));

		TSharedPtr<FArcHeightmapGraphSchemaAction_NewNode> Action =
			MakeShared<FArcHeightmapGraphSchemaAction_NewNode>(
				Category,
				FText::FromString(DisplayName),
				FText::FromString(DisplayName),
				Struct);
		ContextMenuBuilder.AddAction(Action);
	}
}

void UArcHeightmapGraphSchema::GetContextMenuActions(
	class UToolMenu* Menu,
	class UGraphNodeContextMenuContext* Context) const
{
	if (Menu == nullptr || Context == nullptr)
	{
		return;
	}

	FToolMenuSection& Section =
		Menu->AddSection(TEXT("ArcHeightmapGraphNodeActions"),
			LOCTEXT("NodeActionsHeader", "Node Actions"));

	Section.AddMenuEntry(FGenericCommands::Get().Delete);
	Section.AddMenuEntry(
		TEXT("BreakLinks"),
		LOCTEXT("BreakLinksLabel", "Break Links"),
		LOCTEXT("BreakLinksTip", "Break all pin links on this node."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda(
			[Context]()
			{
				UEdGraphNode* GraphNode = const_cast<UEdGraphNode*>(Context->Node.Get());
				if (GraphNode != nullptr)
				{
					GraphNode->BreakAllNodeLinks();
				}
			})));
}

FConnectionDrawingPolicy* UArcHeightmapGraphSchema::CreateConnectionDrawingPolicy(
	int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
	const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj) const
{
	return new FArcHeightmapGraphConnectionDrawingPolicy(
		InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

#undef LOCTEXT_NAMESPACE
