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

#include "ComboSystem/SArcComboTreeGraphNode.h"

#include "GraphEditor.h"
#include "Slate/SObjectWidget.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#include "AbilitySystem/Combo/ArcComboComponent.h"
#include "Components/VerticalBox.h"
#include "SGraphPin.h"
#include "Widgets/Images/SImage.h"

class SArcComboTreePin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SArcComboTreePin)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
				   , UEdGraphPin* InPin);

protected:
	// Begin SGraphPin interface
	virtual TSharedRef<SWidget> GetDefaultValueWidget() override;

	// End SGraphPin interface

	const FSlateBrush* GetPinBorder() const;
};

void SArcComboTreePin::Construct(const FArguments& InArgs
								 , UEdGraphPin* InPin)
{
	this->SetCursor(EMouseCursor::Default);

	bShowLabel = true;

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	check(Schema);

	// Set up a hover for pins that is tinted the color of the pin.
	SBorder::Construct(SBorder::FArguments().BorderImage(this
		, &SArcComboTreePin::GetPinBorder).BorderBackgroundColor(this
		, &SArcComboTreePin::GetPinColor).OnMouseButtonDown(this
		, &SArcComboTreePin::OnPinMouseDown).Cursor(this
		, &SArcComboTreePin::GetPinCursor));
}

TSharedRef<SWidget> SArcComboTreePin::GetDefaultValueWidget()
{
	return SNew(STextBlock);
}

const FSlateBrush* SArcComboTreePin::GetPinBorder() const
{
	return (IsHovered())
		   ? FAppStyle::GetBrush(TEXT("Graph.StateNode.Pin.BackgroundHovered"))
		   : FAppStyle::GetBrush(TEXT("Graph.StateNode.Pin.Background"));
}

/////////////////////////////////////////////////////
// SArcComboTreeGraphNode

void SArcComboTreeGraphNode::Construct(const FArguments& InArgs
									   , UArcComboGraphTreeNode* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

FSlateColor SArcComboTreeGraphNode::GetBorderBackgroundColor() const
{
	FLinearColor InactiveStateColor(0.08f
		, 0.08f
		, 0.08f);
	FLinearColor ActiveStateColorDim(0.4f
		, 0.3f
		, 0.15f);
	FLinearColor ActiveStateColorBright(1.f
		, 0.6f
		, 0.35f);

	return InactiveStateColor;
}

void SArcComboTreeGraphNode::OnMouseEnter(const FGeometry& MyGeometry
										  , const FPointerEvent& MouseEvent)
{
	SGraphNode::OnMouseEnter(MyGeometry
		, MouseEvent);
}

void SArcComboTreeGraphNode::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	SGraphNode::OnMouseLeave(MouseEvent);
}

void SArcComboTreeGraphNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already
	// setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const FSlateBrush* NodeTypeIcon = GetNameIcon();

	FLinearColor TitleShadowColor(0.6f
		, 0.6f
		, 0.6f);
	TSharedPtr<SErrorText> ErrorText;
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle
		, GraphNode);

	this->ContentScale.Bind(this
		, &SGraphNode::GetContentScale);

	const FLinearColor BorderColor(31
		, 31
		, 31);

	this->GetOrAddSlot(ENodeZone::Center).HAlign(HAlign_Center).VAlign(VAlign_Center)[
		SNew(SBorder).BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body")).Padding(0).BorderBackgroundColor(this
			, &SArcComboTreeGraphNode::GetBorderBackgroundColor)[
			SNew(SOverlay) + SOverlay::Slot().Padding(0.f).HAlign(HAlign_Fill).VAlign(VAlign_Fill)[
				SNew(SHorizontalBox) + SHorizontalBox::Slot().HAlign(HAlign_Fill)[
					SNew(SBox).MaxDesiredWidth(15.f)[
						SNew(SBorder).HAlign(HAlign_Fill).VAlign(VAlign_Fill).Padding(0).
									  BorderBackgroundColor(BorderColor).ForegroundColor(BorderColor)[SAssignNew(
							LeftNodeBox
							, SVerticalBox)]]] + SHorizontalBox::Slot()[
					SNew(SBorder).HAlign(HAlign_Fill).VAlign(VAlign_Fill)] + SHorizontalBox::Slot().HAlign(HAlign_Fill)[
					SNew(SBox).MaxDesiredWidth(15.f)[
						SNew(SBorder).HAlign(HAlign_Fill).VAlign(VAlign_Fill).BorderBackgroundColor(BorderColor).
									  ForegroundColor(BorderColor)[SAssignNew(RightNodeBox
							, SVerticalBox)]]]]
			// STATE NAME AREA
			+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(10.0f)[
				SNew(SBorder).BorderImage(FAppStyle::GetBrush("Graph.StateNode.ColorSpill")).
							  BorderBackgroundColor(TitleShadowColor).HAlign(HAlign_Center).VAlign(VAlign_Center).
							  Visibility(EVisibility::SelfHitTestInvisible)[
					SNew(SHorizontalBox) + SHorizontalBox::Slot().AutoWidth()[
						// POPUP ERROR MESSAGE
						SAssignNew(ErrorText
							, SErrorText).BackgroundColor(this
							, &SArcComboTreeGraphNode::GetErrorColor).ToolTipText(this
							, &SArcComboTreeGraphNode::GetErrorMsgToolTip)] + SHorizontalBox::Slot().AutoWidth().
																									 VAlign(
																										 VAlign_Center)[
						SNew(SImage).Image(NodeTypeIcon)] + SHorizontalBox::Slot().Padding(FMargin(4.0f
						, 0.0f
						, 4.0f
						, 0.0f))[SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight()[SAssignNew(InlineEditableText
									 , SInlineEditableTextBlock).Style(FAppStyle::Get()
																    , "Graph.StateNode.NodeTitleInlineEditableText")
															    .Text(NodeTitle.Get()
																    , &SNodeTitle::GetHeadTitle)
															    .OnVerifyTextChanged(this
																    , &
																    SArcComboTreeGraphNode::OnVerifyNameTextChanged)
															    .OnTextCommitted(this
																    , &
																    SArcComboTreeGraphNode::OnNameTextCommited)
															    .IsReadOnly(this
																    , &
																    SArcComboTreeGraphNode::IsNameReadOnly)
															    .IsSelected(this
																    , &
																    SArcComboTreeGraphNode::IsSelectedExclusively)]
								 + SVerticalBox::Slot().AutoHeight()[NodeTitle.ToSharedRef()]]]]]];

	ErrorReporting = ErrorText;
	ErrorReporting->SetError(ErrorMsg);
	CreatePinWidgets();
}

void SArcComboTreeGraphNode::CreatePinWidgets()
{
	UArcComboGraphTreeNode* StateNode = CastChecked<UArcComboGraphTreeNode>(GraphNode);

	UEdGraphPin* OutputPin = StateNode->GetOutputPin();
	if (!OutputPin->bHidden)
	{
		TSharedPtr<SGraphPin> NewPin = SNew(SArcComboTreePin
			, OutputPin);
		NewPin->SetIsEditable(true);

		NewPin->SetOwner(SharedThis(this));
		RightNodeBox->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Fill).FillHeight(1.0f)[NewPin.ToSharedRef()];
		OutputPins.Add(NewPin.ToSharedRef());
	}

	UEdGraphPin* InputPin = StateNode->GetInputPin();
	if (!InputPin->bHidden)
	{
		TSharedPtr<SGraphPin> NewPin = SNew(SArcComboTreePin
			, InputPin);

		NewPin->SetOwner(SharedThis(this));
		NewPin->SetIsEditable(true);

		LeftNodeBox->AddSlot().HAlign(HAlign_Fill).VAlign(VAlign_Fill).FillHeight(1.0f)[NewPin.ToSharedRef()];
		InputPins.Add(NewPin.ToSharedRef());
	}
}

TSharedPtr<SToolTip> SArcComboTreeGraphNode::GetComplexTooltip()
{
	return SGraphNode::GetComplexTooltip();
}

FText SArcComboTreeGraphNode::GetPreviewCornerText() const
{
	return FText::FromString(FString("None"));
	// FText::Format(NSLOCTEXT("SArcComboTreeGraphNode", "PreviewCornerStateText",
	// "state"));
}

const FSlateBrush* SArcComboTreeGraphNode::GetNameIcon() const
{
	return FAppStyle::GetBrush(TEXT("Graph.StateNode.Icon"));
}

#undef LOCTEXT_NAMESPACE
