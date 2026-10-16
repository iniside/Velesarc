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

#include "RecipeGraph/ArcRecipeGraphSchema.h"

#include "RecipeGraph/ArcRecipeGraph.h"
#include "RecipeGraph/ArcRecipeGraphConnectionDrawingPolicy.h"
#include "RecipeGraph/ArcRecipeGraphNode.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRecipeGraphSchema)

#define LOCTEXT_NAMESPACE "ArcRecipeGraphSchema"

// ---------------------------------------------------------------------------
// File-scoped helper namespace (unique across the project)
// ---------------------------------------------------------------------------

namespace ArcRecipeGraphSchemaInternal
{
	/**
	 * Returns true if traversing downstream from StartNode (following output pins
	 * through linked input pins) can reach TargetNode — used for cycle detection.
	 * BFS; visited set prevents infinite loops on existing cycles.
	 */
	static bool ReachesDownstream(const UArcRecipeGraphNode* StartNode, const UArcRecipeGraphNode* TargetNode)
	{
		if (StartNode == nullptr || TargetNode == nullptr)
		{
			return false;
		}

		TSet<const UArcRecipeGraphNode*> Visited;
		TArray<const UArcRecipeGraphNode*> Queue;
		Queue.Add(StartNode);

		while (Queue.Num() > 0)
		{
			const UArcRecipeGraphNode* Current = Queue.Pop(EAllowShrinking::No);
			if (Current == nullptr)
			{
				continue;
			}
			if (Visited.Contains(Current))
			{
				continue;
			}
			Visited.Add(Current);

			if (Current == TargetNode)
			{
				return true;
			}

			// Walk every output pin → every linked input pin → owner node.
			for (const UEdGraphPin* Pin : Current->Pins)
			{
				if (Pin == nullptr || Pin->Direction != EGPD_Output)
				{
					continue;
				}
				for (const UEdGraphPin* Linked : Pin->LinkedTo)
				{
					if (Linked == nullptr)
					{
						continue;
					}
					UArcRecipeGraphNode* NextNode =
						Cast<UArcRecipeGraphNode>(Linked->GetOwningNodeUnchecked());
					if (NextNode != nullptr && !Visited.Contains(NextNode))
					{
						Queue.Add(NextNode);
					}
				}
			}
		}

		return false;
	}

	/**
	 * Normalises A/B into OutputPin / InputPin by direction.
	 * OutputPin receives the EGPD_Output pin; InputPin the EGPD_Input pin.
	 * Either may be nullptr if both pins share the same direction.
	 */
	static void CategorizePinsByDirection(
		const UEdGraphPin* A,
		const UEdGraphPin* B,
		const UEdGraphPin*& OutInputPin,
		const UEdGraphPin*& OutOutputPin)
	{
		OutInputPin  = nullptr;
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

} // namespace ArcRecipeGraphSchemaInternal

// ---------------------------------------------------------------------------
// FArcRecipeGraphSchemaAction_Create
// ---------------------------------------------------------------------------

UEdGraphNode* FArcRecipeGraphSchemaAction_Create::PerformAction(
	UEdGraph* ParentGraph,
	UEdGraphPin* /*FromPin*/,
	const FVector2f& /*Location*/,
	bool /*bSelectNewNode*/)
{
	UArcRecipeGraph* RG = Cast<UArcRecipeGraph>(ParentGraph);
	if (RG != nullptr)
	{
		if (bIsRecipe)
		{
			RG->OnCreateRecipe.ExecuteIfBound();
		}
		else
		{
			RG->OnCreateItem.ExecuteIfBound();
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// UArcRecipeGraphSchema
// ---------------------------------------------------------------------------

const FPinConnectionResponse UArcRecipeGraphSchema::CanCreateConnection(
	const UEdGraphPin* A,
	const UEdGraphPin* B) const
{
	const UEdGraphPin* InputPin  = nullptr;
	const UEdGraphPin* OutputPin = nullptr;
	ArcRecipeGraphSchemaInternal::CategorizePinsByDirection(A, B, InputPin, OutputPin);

	if (InputPin == nullptr || OutputPin == nullptr)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
			LOCTEXT("SameDir", "Connect an output to an input."));
	}

	if (InputPin->GetOwningNodeUnchecked() == OutputPin->GetOwningNodeUnchecked())
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
			LOCTEXT("SelfLoop", "Cannot connect a node to itself."));
	}

	const UArcRecipeGraphNode* TargetNode =
		Cast<UArcRecipeGraphNode>(InputPin->GetOwningNodeUnchecked());
	if (TargetNode == nullptr || TargetNode->NodeType != EArcRecipeGraphNodeType::Recipe)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
			LOCTEXT("NotRecipe", "Only recipes accept ingredients."));
	}

	const UArcRecipeGraphNode* SourceNode =
		Cast<UArcRecipeGraphNode>(OutputPin->GetOwningNodeUnchecked());
	if (ArcRecipeGraphSchemaInternal::ReachesDownstream(TargetNode, SourceNode))
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW,
			LOCTEXT("Cycle", "Would create a dependency cycle."));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE,
		LOCTEXT("AddDep", "Add as recipe ingredient"));
}

bool UArcRecipeGraphSchema::TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const
{
	const UEdGraphPin* InputPin  = nullptr;
	const UEdGraphPin* OutputPin = nullptr;
	ArcRecipeGraphSchemaInternal::CategorizePinsByDirection(A, B, InputPin, OutputPin);

	if (InputPin == nullptr || OutputPin == nullptr)
	{
		return false;
	}

	UArcRecipeGraphNode* SourceNode =
		Cast<UArcRecipeGraphNode>(OutputPin->GetOwningNodeUnchecked());
	UArcRecipeGraphNode* TargetNode =
		Cast<UArcRecipeGraphNode>(InputPin->GetOwningNodeUnchecked());

	if (SourceNode == nullptr || TargetNode == nullptr)
	{
		return false;
	}

	UArcRecipeGraph* RG = Cast<UArcRecipeGraph>(TargetNode->GetGraph());
	if (RG != nullptr)
	{
		RG->OnAddDependency.ExecuteIfBound(SourceNode, TargetNode);
	}

	// Return false — no engine pin link created; the view rebuilds from the data model.
	return false;
}

void UArcRecipeGraphSchema::GetContextMenuActions(
	class UToolMenu* Menu,
	class UGraphNodeContextMenuContext* Context) const
{
	if (Menu == nullptr || Context == nullptr)
	{
		return;
	}

	// Only add recipe-specific actions when a Recipe node is right-clicked.
	UArcRecipeGraph* RecipeGraph =
		Cast<UArcRecipeGraph>(const_cast<UEdGraph*>(Context->Graph.Get()));
	UArcRecipeGraphNode* RecipeNode =
		Cast<UArcRecipeGraphNode>(const_cast<UEdGraphNode*>(Context->Node.Get()));

	if (RecipeGraph == nullptr || RecipeNode == nullptr)
	{
		return;
	}

	if (RecipeNode->NodeType != EArcRecipeGraphNodeType::Recipe)
	{
		return;
	}

	FToolMenuSection& Section =
		Menu->AddSection(TEXT("ArcRecipeGraphNodeActions"),
			LOCTEXT("NodeActionsHeader", "Recipe Actions"));

	// Capture raw pointers — the menu is transient and never outlives the graph.
	FPrimaryAssetId CapturedAssetId = RecipeNode->AssetId;

	Section.AddMenuEntry(
		TEXT("IsolateRecipe"),
		LOCTEXT("IsolateRecipeLabel", "Isolate Dependencies"),
		LOCTEXT("IsolateRecipeTip",
			"Show only this recipe's transitive upstream and downstream."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda(
			[RecipeGraph, CapturedAssetId]()
			{
				RecipeGraph->OnIsolateRecipe.ExecuteIfBound(CapturedAssetId);
			})));

	Section.AddMenuEntry(
		TEXT("ShowAll"),
		LOCTEXT("ShowAllLabel", "Show All"),
		LOCTEXT("ShowAllTip", "Restore the full recipe dependency graph."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda(
			[RecipeGraph]()
			{
				RecipeGraph->OnShowAll.ExecuteIfBound();
			})));

	Section.AddMenuEntry(
		TEXT("OpenRecipeAsset"),
		LOCTEXT("OpenAssetLabel", "Open Recipe Asset"),
		LOCTEXT("OpenAssetTip", "Open this recipe definition in the asset editor."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda(
			[RecipeGraph, CapturedAssetId]()
			{
				RecipeGraph->OnOpenAsset.ExecuteIfBound(CapturedAssetId);
			})));
}

void UArcRecipeGraphSchema::GetGraphContextActions(
	struct FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	// Only add background actions when not dragging from a pin.
	if (ContextMenuBuilder.FromPin != nullptr)
	{
		return;
	}

	TSharedPtr<FArcRecipeGraphSchemaAction_Create> ItemAction =
		MakeShared<FArcRecipeGraphSchemaAction_Create>(
			LOCTEXT("CreateItem",    "Create Item"),
			LOCTEXT("CreateItemTip", "Create a new item definition asset."),
			false);
	ContextMenuBuilder.AddAction(ItemAction);

	TSharedPtr<FArcRecipeGraphSchemaAction_Create> RecipeAction =
		MakeShared<FArcRecipeGraphSchemaAction_Create>(
			LOCTEXT("CreateRecipe",    "Create Recipe"),
			LOCTEXT("CreateRecipeTip", "Create a new recipe definition asset."),
			true);
	ContextMenuBuilder.AddAction(RecipeAction);
}

FConnectionDrawingPolicy* UArcRecipeGraphSchema::CreateConnectionDrawingPolicy(
	int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
	const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements,
	UEdGraph* InGraphObj) const
{
	return new FArcRecipeGraphConnectionDrawingPolicy(
		InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements, InGraphObj);
}

#undef LOCTEXT_NAMESPACE
