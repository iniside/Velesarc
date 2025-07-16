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

#include "ComboSystem/ArcComboTreeGraphSchema.h"

#include "AbilitySystem/Combo/ArcComboComponent.h"
#include "ComboSystem/ArcEdGraph_ComboTree.h"
#include "EdGraph/EdGraphPin.h"

#include "ComboSystem/ArcComboGraphTreeNode.h"

#define SNAP_GRID (16)
#define DROP_NODE_GRID_WIDTH (4)

namespace
{
	// Maximum distance a drag can be off a node edge to require 'push off' from node
	const int32 NodeDistance = 60;
} // namespace

UEdGraphNode* FArcComboTreeSchemaAction::PerformAction(class UEdGraph* ParentGraph
													   , UEdGraphPin* FromPin
													   , const FVector2D Location
													   , bool bSelectNewNode /* = true*/
	)
{
	UEdGraphNode* ResultNode = nullptr;

	// If there is a template, we actually use it
	if (NodeTemplate != nullptr)
	{
		NodeTemplate->SetFlags(RF_Transactional);
		if (FromPin)
		{
			FromPin->Modify();
		}
		// set outer to be the graph so it doesn't go away
		NodeTemplate->Rename(nullptr
			, ParentGraph
			, REN_NonTransactional);
		ParentGraph->AddNode(NodeTemplate
			, true
			, bSelectNewNode);

		NodeTemplate->CreateNewGuid();
		NodeTemplate->PostPlacedNewNode();
		NodeTemplate->AllocateDefaultPins();

		UArcComboGraphTreeNode* Node = Cast<UArcComboGraphTreeNode>(NodeTemplate);
		UArcEdGraph_ComboTree* EdGraph = Cast<UArcEdGraph_ComboTree>(ParentGraph);
		FGuid NodeId = EdGraph->GetAbilityTree()->AddNewNode();
		Node->NodeId = NodeId;

		NodeTemplate->AutowireNewNode(FromPin);
		bool bConnection = false;

		if (FromPin)
		{
			FromPin->Modify();
		}
		// For input pins, new node will generally overlap node being dragged off
		// Work out if we want to visually push away from connected node
		int32 XLocation = Location.X;
		if (FromPin && FromPin->Direction == EGPD_Input)
		{
			UEdGraphNode* PinNode = FromPin->GetOwningNode();
			const float XDelta = FMath::Abs(PinNode->NodePosX - Location.X);

			if (XDelta < NodeDistance)
			{
				// Set location to edge of current node minus the max move distance
				// to force node to push off from connect node enough to give selection
				// handle
				XLocation = PinNode->NodePosX - NodeDistance;
			}
		}

		NodeTemplate->NodePosX = XLocation;
		NodeTemplate->NodePosY = Location.Y;
		NodeTemplate->SnapToGrid(SNAP_GRID);

		ResultNode = NodeTemplate;
	}
	ParentGraph->NotifyGraphChanged();
	return ResultNode;
}

UEdGraphNode* FArcComboTreeSchemaAction::PerformAction(class UEdGraph* ParentGraph
													   , TArray<UEdGraphPin*>& FromPins
													   , const FVector2D Location
													   , bool bSelectNewNode /* = true*/
	)
{
	UEdGraphNode* ResultNode = nullptr;

	if (FromPins.Num() > 0)
	{
		ResultNode = PerformAction(ParentGraph
			, FromPins[0]
			, Location
			, bSelectNewNode);

		// Try autowiring the rest of the pins
		for (int32 Index = 1; Index < FromPins.Num(); ++Index)
		{
			ResultNode->AutowireNewNode(FromPins[Index]);
		}
	}
	else
	{
		ResultNode = PerformAction(ParentGraph
			, nullptr
			, Location
			, bSelectNewNode);
	}

	return ResultNode;
}

void FArcComboTreeSchemaAction::AddReferencedObjects(FReferenceCollector& Collector)
{
	FEdGraphSchemaAction::AddReferencedObjects(Collector);

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd
	// while the action array is around
	Collector.AddReferencedObject(NodeTemplate);
}

FArcComboTreeGraphConnectionDrawingPolicy::FArcComboTreeGraphConnectionDrawingPolicy(int32 InBackLayerID
																					 , int32 InFrontLayerID
																					 , float ZoomFactor
																					 , const FSlateRect& InClippingRect
																					 , FSlateWindowElementList&
																					 InDrawElements
																					 , UEdGraph* InGraphObj)
	: FConnectionDrawingPolicy(InBackLayerID
		, InFrontLayerID
		, ZoomFactor
		, InClippingRect
		, InDrawElements)
	, GraphObj(InGraphObj)
{
}

void FArcComboTreeGraphConnectionDrawingPolicy::DetermineWiringStyle(UEdGraphPin* OutputPin
																	 , UEdGraphPin* InputPin
																	 ,
																	 /*inout*/ FConnectionParams& Params)
{
	Params.AssociatedPin1 = OutputPin;
	Params.AssociatedPin2 = InputPin;
	Params.WireThickness = 1.5f;

	Params.WireColor = FLinearColor::White;

	UArcComboGraphTreeNode* FromNode = OutputPin ? Cast<UArcComboGraphTreeNode>(OutputPin->GetOwningNode()) : NULL;
	UArcComboGraphTreeNode* ToNode = InputPin ? Cast<UArcComboGraphTreeNode>(InputPin->GetOwningNode()) : NULL;
	if (ToNode && FromNode)
	{
#ifdef TODO_CONVERSATION_EDITOR //@TODO: CONVERSATION: Find uses of
								//TODO_CONVERSATION_EDITOR
		if ((ToNode->bDebuggerMarkCurrentlyActive
			 && FromNode->bDebuggerMarkCurrentlyActive)
			|| (ToNode->bDebuggerMarkPreviouslyActive
				&& FromNode->bDebuggerMarkPreviouslyActive))
		{
			Params.WireThickness = 10.0f;
			Params.bDrawBubbles = true;
		}
		else if (FConversationDebugger::IsPlaySessionPaused())
		{
			UConversationGraphNode* FirstToNode = ToNode;
			int32 FirstPathIdx = ToNode->DebuggerSearchPathIndex;
			for (int32 i = 0; i < ToNode->Decorators.Num(); i++)
			{
				UConversationGraphNode* TestNode = ToNode->Decorators[i];
				if (TestNode->DebuggerSearchPathIndex != INDEX_NONE
					&& (TestNode->bDebuggerMarkSearchSucceeded
						|| TestNode->bDebuggerMarkSearchFailed))
				{
					if (TestNode->DebuggerSearchPathIndex < FirstPathIdx
						|| FirstPathIdx == INDEX_NONE)
					{
						FirstPathIdx = TestNode->DebuggerSearchPathIndex;
						FirstToNode = TestNode;
					}
				}
			}

			if (FirstToNode->bDebuggerMarkSearchSucceeded
				|| FirstToNode->bDebuggerMarkSearchFailed)
			{
				Params.WireThickness = 5.0f;
				Params.WireColor = FirstToNode->bDebuggerMarkSearchSucceeded
					? ConversationEditorColors::Debugger::SearchSucceeded
					: ConversationEditorColors::Debugger::SearchFailed;

				// Use the bUserFlag1 flag to indicate that we need to reverse the
				// direction of connection (used by debugger)
				Params.bUserFlag1 = true;
			}
		}
#endif
	}

	const bool bDeemphasizeUnhoveredPins = HoveredPins.Num() > 0;
	if (bDeemphasizeUnhoveredPins)
	{
		ApplyHoverDeemphasis(OutputPin
			, InputPin
			,
			/*inout*/ Params.WireThickness
			,
			/*inout*/ Params.WireColor);
	}
}

void FArcComboTreeGraphConnectionDrawingPolicy::Draw(TMap<TSharedRef<SWidget>, FArrangedWidget>& InPinGeometries
													 , FArrangedChildren& ArrangedNodes)
{
	// Build an acceleration structure to quickly find geometry for the nodes
	NodeWidgetMap.Empty();
	for (int32 NodeIndex = 0; NodeIndex < ArrangedNodes.Num(); ++NodeIndex)
	{
		FArrangedWidget& CurWidget = ArrangedNodes[NodeIndex];
		TSharedRef<SGraphNode> ChildNode = StaticCastSharedRef<SGraphNode>(CurWidget.Widget);
		NodeWidgetMap.Add(ChildNode->GetNodeObj()
			, NodeIndex);
	}

	// Now draw
	FConnectionDrawingPolicy::Draw(InPinGeometries
		, ArrangedNodes);
}

void FArcComboTreeGraphConnectionDrawingPolicy::DrawPreviewConnector(const FGeometry& PinGeometry
																	 , const FVector2D& StartPoint
																	 , const FVector2D& EndPoint
																	 , UEdGraphPin* Pin)
{
	FConnectionParams Params;
	DetermineWiringStyle(Pin
		, nullptr
		, /*inout*/ Params);

	if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		DrawSplineWithArrow(FGeometryHelper::FindClosestPointOnGeom(PinGeometry
				, EndPoint)
			, EndPoint
			, Params);
	}
	else
	{
		DrawSplineWithArrow(FGeometryHelper::FindClosestPointOnGeom(PinGeometry
				, StartPoint)
			, StartPoint
			, Params);
	}
}

void FArcComboTreeGraphConnectionDrawingPolicy::DrawSplineWithArrow(const FVector2D& StartAnchorPoint
																	, const FVector2D& EndAnchorPoint
																	, const FConnectionParams& Params)
{
	// bUserFlag1 indicates that we need to reverse the direction of connection (used by
	// debugger)
	const FVector2D& P0 = StartAnchorPoint; // Params.bUserFlag1 ? EndAnchorPoint : ;
	const FVector2D& P1 = EndAnchorPoint; // Params.bUserFlag1 ? StartAnchorPoint : ;

	Internal_DrawLineWithArrow(P0
		, P1
		, Params);
}

void FArcComboTreeGraphConnectionDrawingPolicy::Internal_DrawLineWithArrow(const FVector2D& StartAnchorPoint
																		   , const FVector2D& EndAnchorPoint
																		   , const FConnectionParams& Params)
{
	//@TODO: Should this be scaled by zoom factor?
	const float LineSeparationAmount = 4.5f;

	const FVector2D DeltaPos = EndAnchorPoint - StartAnchorPoint;
	const FVector2D UnitDelta = DeltaPos.GetSafeNormal();
	const FVector2D Normal = FVector2D(DeltaPos.Y
		, -DeltaPos.X).GetSafeNormal();

	// Come up with the final start/end points
	const FVector2D DirectionBias = Normal * LineSeparationAmount;
	const FVector2D LengthBias = ArrowRadius.X * UnitDelta;
	const FVector2D StartPoint = StartAnchorPoint + DirectionBias + LengthBias;
	const FVector2D EndPoint = EndAnchorPoint + DirectionBias - LengthBias;

	// Draw a line/spline
	DrawConnection(WireLayerID
		, StartPoint
		, EndPoint
		, Params);

	// Draw the arrow
	const FVector2D ArrowDrawPos = EndPoint - ArrowRadius;
	const float AngleInRadians = FMath::Atan2(DeltaPos.Y
		, DeltaPos.X);

	FSlateDrawElement::MakeRotatedBox(DrawElementsList
		, ArrowLayerID
		, FPaintGeometry(ArrowDrawPos
			, ArrowImage->ImageSize * ZoomFactor
			, ZoomFactor)
		, ArrowImage
		, ESlateDrawEffect::None
		, AngleInRadians
		, TOptional<FVector2D>()
		, FSlateDrawElement::RelativeToElement
		, Params.WireColor);
}

void FArcComboTreeGraphConnectionDrawingPolicy::DrawSplineWithArrow(const FGeometry& StartGeom
																	, const FGeometry& EndGeom
																	, const FConnectionParams& Params)
{
	// Get a reasonable seed point (halfway between the boxes)
	const FVector2D StartCenter = FGeometryHelper::CenterOf(StartGeom);
	const FVector2D EndCenter = FGeometryHelper::CenterOf(EndGeom);
	const FVector2D SeedPoint = (StartCenter + EndCenter) * 0.5f;

	// Find the (approximate) closest points between the two boxes
	const FVector2D StartAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(StartGeom
		, SeedPoint);
	const FVector2D EndAnchorPoint = FGeometryHelper::FindClosestPointOnGeom(EndGeom
		, SeedPoint);

	DrawSplineWithArrow(StartAnchorPoint
		, EndAnchorPoint
		, Params);
}

FVector2D FArcComboTreeGraphConnectionDrawingPolicy::ComputeSplineTangent(const FVector2D& Start
																		  , const FVector2D& End) const
{
	const FVector2D Delta = End - Start;
	const FVector2D NormDelta = Delta.GetSafeNormal();

	return NormDelta;
}

void UArcComboTreeGraphSchema::GetActionList(TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions
											 , const UEdGraph* Graph
											 , EMkSchemaActionType ActionType) const
{
	if (const UArcEdGraph_ComboTree* AbilityTreeGraph = Cast<UArcEdGraph_ComboTree>(Graph))
	{
		if (UArcComboGraph* AbilityTree = AbilityTreeGraph->GetAbilityTree())
		{
			AddAction<UArcComboGraphTreeNode>(TEXT("Add Entry Node")
				, TEXT("Add entry")
				, OutActions
				, AbilityTree);
		}
	}
}

void UArcComboTreeGraphSchema::DroppedAssetsOnGraph(const TArray<struct FAssetData>& Assets
													, const FVector2D& GraphPosition
													, UEdGraph* Graph) const
{
	UArcEdGraph_ComboTree* AbilityTree = Cast<UArcEdGraph_ComboTree>(Graph);
	FVector2D OffsetSize = FVector2D(SNAP_GRID);
	// AbilityTree ? AbilityTree->GetAbilityTree()->SlotSize : FVector2D(SNAP_GRID);

	for (int32 Index = 0; Index < Assets.Num(); Index++)
	{
		int32 RowNr = Index % DROP_NODE_GRID_WIDTH;
		int32 CollumnNr = Index / DROP_NODE_GRID_WIDTH;

		UArcComboGraphTreeNode* NewNode = NewObject<UArcComboGraphTreeNode>(Graph);
		NewNode->SetFlags(RF_Transactional);

		Graph->AddNode(NewNode
			, true
			, false);

		NewNode->CreateNewGuid();
		NewNode->PostPlacedNewNode();
		NewNode->AllocateDefaultPins();

		NewNode->NodePosX = GraphPosition.X + (RowNr * OffsetSize.X);
		NewNode->NodePosY = GraphPosition.Y + (CollumnNr * OffsetSize.Y);
		NewNode->SnapToGrid(SNAP_GRID);
	}
}

void UArcComboTreeGraphSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets
														  , const UEdGraph* HoverGraph
														  , FString& OutTooltipText
														  , bool& OutOkIcon) const
{
	OutOkIcon = false;
}

void UArcComboTreeGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
	const UEdGraph* Graph = ContextMenuBuilder.CurrentGraph;
	TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
	GetActionList(Actions
		, Graph
		, EMkSchemaActionType::All);
	for (TSharedPtr<FEdGraphSchemaAction> Action : Actions)
	{
		ContextMenuBuilder.AddAction(Action);
	}
}

const FPinConnectionResponse UArcComboTreeGraphSchema::CanCreateConnection(const UEdGraphPin* A
																		   , const UEdGraphPin* B) const
{
	// Make sure the input is connecting to an output
	if (A->Direction == B->Direction)
	{
		return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW
			, TEXT("Not allowed"));
	}

	return FPinConnectionResponse(CONNECT_RESPONSE_MAKE
		, TEXT(""));
}

FLinearColor UArcComboTreeGraphSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return FColor::Yellow;
}

bool UArcComboTreeGraphSchema::ShouldHidePinDefaultValue(UEdGraphPin* Pin) const
{
	return false;
}

bool UArcComboTreeGraphSchema::TryCreateConnection(UEdGraphPin* A
												   , UEdGraphPin* B) const
{
	bool ConnectionMade = UEdGraphSchema::TryCreateConnection(A
		, B);
	if (ConnectionMade)
	{
		UEdGraphPin* OutputPin = (A->Direction == EEdGraphPinDirection::EGPD_Output) ? A : B;
		UArcComboGraphTreeNode* OutputNode = Cast<UArcComboGraphTreeNode>(OutputPin->GetOwningNode());
		if (OutputNode)
		{
			OutputNode->GetGraph()->NotifyGraphChanged();
		}
	}

	return ConnectionMade;
}

FConnectionDrawingPolicy* UArcComboTreeGraphSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID
																				  , int32 InFrontLayerID
																				  , float InZoomFactor
																				  , const FSlateRect& InClippingRect
																				  , FSlateWindowElementList&
																				  InDrawElements
																				  , UEdGraph* InGraphObj) const
{
	return new FArcComboTreeGraphConnectionDrawingPolicy(InBackLayerID
		, InFrontLayerID
		, InZoomFactor
		, InClippingRect
		, InDrawElements
		, InGraphObj);
}
