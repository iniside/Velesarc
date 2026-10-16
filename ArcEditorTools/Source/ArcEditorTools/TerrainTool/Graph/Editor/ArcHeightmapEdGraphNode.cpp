// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraphNode.h"

#include "EdGraph/EdGraphPin.h"
#include "StructUtils/InstancedStruct.h"
#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "TerrainTool/Graph/ArcHeightmapNodes.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcHeightmapEdGraphNode)

const FName ArcHeightmapPins::HeightField(TEXT("HeightField"));

// ---------------------------------------------------------------------------
// File-scoped helpers (unique across the project)
// ---------------------------------------------------------------------------

namespace ArcHeightmapEdGraphNodeInternal
{
	/** Resolve the mirrored FArcHeightmapNode for the given NodeId from the owning graph's source asset. */
	static const FArcHeightmapNode* ResolveNode(const UEdGraphNode* GraphNode, const FGuid& NodeId)
	{
		const UArcHeightmapEdGraph* OwningGraph = Cast<UArcHeightmapEdGraph>(GraphNode->GetGraph());
		if (OwningGraph == nullptr)
		{
			return nullptr;
		}

		const UArcHeightmapGraph* SourceAsset = OwningGraph->SourceAsset.Get();
		if (SourceAsset == nullptr)
		{
			return nullptr;
		}

		for (const FInstancedStruct& Entry : SourceAsset->Nodes)
		{
			const FArcHeightmapNode* Node = Entry.GetPtr<FArcHeightmapNode>();
			if (Node != nullptr && Node->NodeId == NodeId)
			{
				return Node;
			}
		}

		return nullptr;
	}

	/** Resolve the UScriptStruct of the mirrored node for the given NodeId. */
	static const UScriptStruct* ResolveStruct(const UEdGraphNode* GraphNode, const FGuid& NodeId)
	{
		const UArcHeightmapEdGraph* OwningGraph = Cast<UArcHeightmapEdGraph>(GraphNode->GetGraph());
		if (OwningGraph == nullptr)
		{
			return nullptr;
		}

		const UArcHeightmapGraph* SourceAsset = OwningGraph->SourceAsset.Get();
		if (SourceAsset == nullptr)
		{
			return nullptr;
		}

		for (const FInstancedStruct& Entry : SourceAsset->Nodes)
		{
			const FArcHeightmapNode* Node = Entry.GetPtr<FArcHeightmapNode>();
			if (Node != nullptr && Node->NodeId == NodeId)
			{
				return Entry.GetScriptStruct();
			}
		}

		return nullptr;
	}

} // namespace ArcHeightmapEdGraphNodeInternal

// ---------------------------------------------------------------------------
// UEdGraphNode overrides
// ---------------------------------------------------------------------------

void UArcHeightmapEdGraphNode::AllocateDefaultPins()
{
	const FArcHeightmapNode* Node = ArcHeightmapEdGraphNodeInternal::ResolveNode(this, NodeId);
	if (Node == nullptr)
	{
		return;
	}

	const int32 InputCount = Node->GetInputCount();
	for (int32 Index = 0; Index < InputCount; ++Index)
	{
		CreatePin(
			EGPD_Input,
			ArcHeightmapPins::HeightField,
			NAME_None,
			FName(*FString::Printf(TEXT("In%d"), Index)));
	}

	CreatePin(EGPD_Output, ArcHeightmapPins::HeightField, FName(TEXT("Out")));
}

FText UArcHeightmapEdGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UScriptStruct* Struct = ArcHeightmapEdGraphNodeInternal::ResolveStruct(this, NodeId);
	if (Struct == nullptr)
	{
		return NSLOCTEXT("ArcHeightmapGraph", "UnknownNode", "Unknown");
	}

	if (Struct == FArcHeightmapNode_Output::StaticStruct())
	{
		return NSLOCTEXT("ArcHeightmapGraph", "OutputNode", "Output");
	}
	if (Struct == FArcHeightmapNode_Constant::StaticStruct())
	{
		return NSLOCTEXT("ArcHeightmapGraph", "ConstantNode", "Constant");
	}

	if (Struct == FArcHeightmapNode_NoiseStandard::StaticStruct())
	{
		return NSLOCTEXT("ArcHeightmapGraph", "NoiseStandardNode", "Perlin (Standard)");
	}
	if (Struct == FArcHeightmapNode_NoiseTurbulent::StaticStruct())
	{
		return NSLOCTEXT("ArcHeightmapGraph", "NoiseTurbulentNode", "Perlin (Turbulent)");
	}
	if (Struct == FArcHeightmapNode_NoiseRidge::StaticStruct())
	{
		return NSLOCTEXT("ArcHeightmapGraph", "NoiseRidgeNode", "Perlin (Ridge)");
	}
	if (Struct == FArcHeightmapNode_ErosionFlow::StaticStruct())
	{
		return NSLOCTEXT("ArcHeightmapGraph", "ErosionFlowNode", "Flow Erosion");
	}
	if (Struct == FArcHeightmapNode_ErosionAdvanced::StaticStruct())
	{
		return NSLOCTEXT("ArcHeightmapGraph", "ErosionAdvancedNode", "Erosion (Advanced)");
	}

	// Strip the "ArcHeightmapNode_" prefix from the struct display name.
	FString DisplayName = Struct->GetDisplayNameText().ToString();
	const FString Prefix(TEXT("ArcHeightmapNode_"));
	DisplayName.RemoveFromStart(Prefix);
	return FText::FromString(DisplayName);
}

FLinearColor UArcHeightmapEdGraphNode::GetNodeTitleColor() const
{
	const UScriptStruct* Struct = ArcHeightmapEdGraphNodeInternal::ResolveStruct(this, NodeId);
	if (Struct == nullptr)
	{
		return FLinearColor(FColor(120, 120, 120));
	}

	if (Struct->IsChildOf(FArcHeightmapNode_NoiseBase::StaticStruct()) || Struct == FArcHeightmapNode_Noise::StaticStruct())
	{
		return FLinearColor(FColor(72, 160, 180));
	}
	if (Struct == FArcHeightmapNode_Erosion::StaticStruct() || Struct->IsChildOf(FArcHeightmapNode_Erosion::StaticStruct()) || Struct == FArcHeightmapNode_ErosionFlow::StaticStruct())
	{
		return FLinearColor(FColor(170, 120, 70));
	}
	if (Struct == FArcHeightmapNode_Math::StaticStruct()
		|| Struct == FArcHeightmapNode_Add::StaticStruct()
		|| Struct == FArcHeightmapNode_Multiply::StaticStruct()
		|| Struct == FArcHeightmapNode_Lerp::StaticStruct())
	{
		return FLinearColor(FColor(110, 120, 150));
	}
	if (Struct == FArcHeightmapNode_Levels::StaticStruct() || Struct == FArcHeightmapNode_Circle::StaticStruct())
	{
		return FLinearColor(FColor(150, 110, 160));
	}
	if (Struct == FArcHeightmapNode_Constant::StaticStruct())
	{
		return FLinearColor(FColor(120, 120, 120));
	}
	if (Struct == FArcHeightmapNode_Output::StaticStruct())
	{
		return FLinearColor(FColor(80, 170, 90));
	}
	if (Struct == FArcHeightmapNode_Voronoi::StaticStruct())
	{
		return FLinearColor(FColor(72, 160, 180));
	}
	if (Struct == FArcHeightmapNode_DomainWarp::StaticStruct())
	{
		return FLinearColor(FColor(72, 160, 180));
	}

	return FLinearColor(FColor(120, 120, 120));
}

bool UArcHeightmapEdGraphNode::CanUserDeleteNode() const
{
	const UScriptStruct* Struct = ArcHeightmapEdGraphNodeInternal::ResolveStruct(this, NodeId);
	if (Struct == FArcHeightmapNode_Output::StaticStruct())
	{
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Pin accessors
// ---------------------------------------------------------------------------

UEdGraphPin* UArcHeightmapEdGraphNode::GetInputPin(int32 Index) const
{
	int32 Found = 0;
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin != nullptr && Pin->Direction == EGPD_Input)
		{
			if (Found == Index)
			{
				return Pin;
			}
			++Found;
		}
	}
	return nullptr;
}

UEdGraphPin* UArcHeightmapEdGraphNode::GetOutputPin() const
{
	for (UEdGraphPin* Pin : Pins)
	{
		if (Pin != nullptr && Pin->Direction == EGPD_Output)
		{
			return Pin;
		}
	}
	return nullptr;
}
