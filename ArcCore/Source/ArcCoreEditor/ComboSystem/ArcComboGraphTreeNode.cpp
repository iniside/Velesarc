/**
 * This file is part of ArcX.
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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

#include "ComboSystem/ArcComboGraphTreeNode.h"
#include "ComboSystem/ArcEdGraph_ComboTree.h"

#include "AbilitySystem/Combo/ArcComboComponent.h"

#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"

#define LOCTEXT_NAMESPACE "UArcComboGraphTreeNode"

UArcComboGraphTreeNode::UArcComboGraphTreeNode()
{
}

void UArcComboGraphTreeNode::PostEditChangeProperty(struct FPropertyChangedEvent& e)
{
	Super::PostEditChangeProperty(e);

	TArray<FName> RefreshNames = {"NodeName", "AssociatedObject"};
	if (e.Property && RefreshNames.Contains(e.Property->GetFName()))
	{
		GetGraph()->NotifyGraphChanged();
	}
}

FText UArcComboGraphTreeNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const FArcComboNode* N = GetAbilityTree()->FindNode(NodeId);
	if (N)
	{
		// const FMkATDF_Name* NameFragment = N->GetFragment<FMkATDF_Name>();
		// if (NameFragment)
		//{
		//	return NameFragment->Name;
		// }
	}

	return FText::FromString("Add Name Fragment");
}

FLinearColor UArcComboGraphTreeNode::GetNodeTitleColor() const
{
	return FLinearColor(0.7f
		, 0.7f
		, 0.7f);
}

void UArcComboGraphTreeNode::NodeConnectionListChanged()
{
	UEdGraphNode::NodeConnectionListChanged();
	GetGraph()->NotifyGraphChanged();
}

UArcComboGraph* UArcComboGraphTreeNode::GetAbilityTree() const
{
	return GetAbilityTreeGraph()->GetAbilityTree();
}

UArcEdGraph_ComboTree* UArcComboGraphTreeNode::GetAbilityTreeGraph() const
{
	return Cast<UArcEdGraph_ComboTree>(GetGraph());
}

UEdGraphPin* UArcComboGraphTreeNode::GetOutputPin() const
{
	return Pins[1];
}

UEdGraphPin* UArcComboGraphTreeNode::GetInputPin() const
{
	return Pins[0];
}

void UArcComboGraphTreeNode::PinConnectionListChanged(UEdGraphPin* Pin)
{
	FArcComboNode* MyNode = GetAbilityTree()->FindNode(NodeId);

	if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		MyNode->ClearAllParents();
		// MyNode->ClearAllChildren();
		for (UEdGraphPin* LTPin : Pin->LinkedTo)
		{
			UArcComboGraphTreeNode* Other = Cast<UArcComboGraphTreeNode>(LTPin->GetOwningNode());
			FArcComboNode* Node = GetAbilityTree()->FindNode(Other->NodeId);

			GetAbilityTree()->AddParent(NodeId
				, Other->NodeId);
		}
	}
	else if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		MyNode->ClearAllChildren();
		// MyNode->ClearAllParents();
		for (UEdGraphPin* LTPin : Pin->LinkedTo)
		{
			UArcComboGraphTreeNode* Other = Cast<UArcComboGraphTreeNode>(LTPin->GetOwningNode());
			FArcComboNode* Node = GetAbilityTree()->FindNode(Other->NodeId);

			GetAbilityTree()->AddChild(NodeId
				, Other->NodeId);
		}
	}
}

void UArcComboGraphTreeNode::AutowireNewNode(UEdGraphPin* FromPin)
{
	if (FromPin == nullptr)
	{
		return;
	}
	FArcComboNode* MyNode = GetAbilityTree()->FindNode(NodeId);
	if (FromPin->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		if (MyNode)
		{
			MyNode->ClearAllChildren();
		}

		UArcComboGraphTreeNode* Other = Cast<UArcComboGraphTreeNode>(FromPin->GetOwningNode());
		GetAbilityTree()->AddParent(Other->NodeId
			, NodeId);
		GetAbilityTree()->AddChild(NodeId
			, Other->NodeId);
	}
	else if (FromPin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		if (MyNode)
		{
			MyNode->ClearAllParents();
		}

		UArcComboGraphTreeNode* Other = Cast<UArcComboGraphTreeNode>(FromPin->GetOwningNode());
		GetAbilityTree()->AddParent(NodeId
			, Other->NodeId);
		GetAbilityTree()->AddChild(Other->NodeId
			, NodeId);
		if (GetSchema()->TryCreateConnection(FromPin
			, GetInputPin()))
		{
			FromPin->GetOwningNode()->NodeConnectionListChanged();
		}
	}
}

void UArcComboGraphTreeNode::AllocateDefaultPins()
{
	CreatePin(EEdGraphPinDirection::EGPD_Input
		, TEXT("AbilityTree")
		, TEXT("In"));
	CreatePin(EEdGraphPinDirection::EGPD_Output
		, TEXT("AbilityTree")
		, TEXT("Out"));
}

void UArcComboGraphTreeNode::DestroyNode()
{
	BreakAllNodeLinks();
	GetAbilityTree()->RemoveNode(NodeId);
	// AssociatedObject->RemoveFromParentNodes();
	Super::DestroyNode();
}

#undef LOCTEXT_NAMESPACE
