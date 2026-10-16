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

#include "RecipeGraph/SArcRecipeGraphNode.h"

#include "RecipeGraph/ArcRecipeGraphNode.h"
#include "RecipeGraph/ArcRecipeGraphData.h"
#include "RecipeGraph/ArcRecipeGraph.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"

// ---------------------------------------------------------------------------
// File-scoped helpers (unique namespace per CLAUDE.md rule)
// ---------------------------------------------------------------------------

namespace SArcRecipeGraphNodeInternal
{
	/** Left/right padding around the member-list text entries. */
	static const FMargin MemberItemPadding(6.0f, 1.0f, 6.0f, 1.0f);

	/** Font used for the TagGroup member-item list. */
	static FSlateFontInfo GetMemberFont()
	{
		return FAppStyle::GetFontStyle(TEXT("Graph.Node.NodeTitleExtraLines"));
	}

} // namespace SArcRecipeGraphNodeInternal

// ---------------------------------------------------------------------------
// SArcRecipeGraphNode
// ---------------------------------------------------------------------------

void SArcRecipeGraphNode::Construct(const FArguments& InArgs, UArcRecipeGraphNode* InNode)
{
	GraphNode = InNode;
	SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

void SArcRecipeGraphNode::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SGraphNode::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const UArcRecipeGraphNode* ArcNode = Cast<UArcRecipeGraphNode>(GraphNode);
	const float TargetOpacity =
		(ArcNode != nullptr && ArcNode->HighlightState == EArcRecipeNodeHighlight::Dimmed)
			? 0.25f
			: 1.0f;

	if (GetRenderOpacity() != TargetOpacity)
	{
		SetRenderOpacity(TargetOpacity);
	}
}

void SArcRecipeGraphNode::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	UArcRecipeGraphNode* ArcNode = Cast<UArcRecipeGraphNode>(GraphNode);
	if (ArcNode == nullptr)
	{
		return;
	}

	if (ArcNode->NodeType != EArcRecipeGraphNodeType::TagGroup)
	{
		return;
	}

	if (ArcNode->GroupMembers.Num() == 0)
	{
		return;
	}

	// Build a small list of member item ids below the pin area.
	TSharedRef<SVerticalBox> MemberBox = SNew(SVerticalBox);

	UArcRecipeGraph* OwningGraph = Cast<UArcRecipeGraph>(ArcNode->GetGraph());

	for (int32 Idx = 0; Idx < ArcNode->GroupMembers.Num(); ++Idx)
	{
		const FText MemberText = ArcNode->GroupMemberTitles.IsValidIndex(Idx)
			? ArcNode->GroupMemberTitles[Idx]
			: FText::FromName(ArcNode->GroupMembers[Idx].PrimaryAssetName);
		const FPrimaryAssetId MemberId = ArcNode->GroupMembers[Idx];

		if (OwningGraph == nullptr)
		{
			MemberBox->AddSlot()
				.AutoHeight()
				.Padding(SArcRecipeGraphNodeInternal::MemberItemPadding)
				[
					SNew(STextBlock)
					.Text(MemberText)
					.Font(SArcRecipeGraphNodeInternal::GetMemberFont())
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f))
				];
		}
		else
		{
			MemberBox->AddSlot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("NoBorder"))
					.Padding(SArcRecipeGraphNodeInternal::MemberItemPadding)
					// Select on mouse-DOWN (mirrors SGraphNodeAI::OnMouseDown): returning Handled here
					// stops SNodePanel from stealing the click for a node-drag, and doing the work on
					// down means we never depend on a mouse-up that has no captured target.
					.OnMouseButtonDown_Lambda([OwningGraph, MemberId](const FGeometry&, const FPointerEvent& E) -> FReply
					{
						if (E.GetEffectingButton() == EKeys::LeftMouseButton)
						{
							OwningGraph->OnSelectMember.ExecuteIfBound(MemberId);
							return FReply::Handled();
						}
						return FReply::Unhandled();
					})
					.OnMouseDoubleClick_Lambda([OwningGraph, MemberId](const FGeometry&, const FPointerEvent& E) -> FReply
					{
						if (E.GetEffectingButton() == EKeys::LeftMouseButton)
						{
							OwningGraph->OnOpenAsset.ExecuteIfBound(MemberId);
							return FReply::Handled();
						}
						return FReply::Unhandled();
					})
					[
						SNew(STextBlock)
						.Text(MemberText)
						.Font(SArcRecipeGraphNodeInternal::GetMemberFont())
						.ColorAndOpacity(TAttribute<FSlateColor>::Create(TAttribute<FSlateColor>::FGetter::CreateLambda(
							[OwningGraph, MemberId]() -> FSlateColor
							{
								const bool bSelected = OwningGraph != nullptr && OwningGraph->SelectedMemberId == MemberId;
								return bSelected
									? FSlateColor(FLinearColor(1.0f, 0.85f, 0.4f, 1.0f))
									: FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
							})))
					]
				];
		}
	}

	MainBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			.Padding(FMargin(0.0f))
			[
				MemberBox
			]
		];
}
