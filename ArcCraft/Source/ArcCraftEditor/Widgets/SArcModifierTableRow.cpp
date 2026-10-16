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

#include "Widgets/SArcModifierTableRow.h"
#include "Widgets/SArcModifierTreeView.h"
#include "Widgets/SArcModifierRowHandle.h"
#include "Widgets/ArcModifierColumnSchema.h"
#include "ArcCraftEditorModule.h"

#include "Widgets/SOverlay.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Layout/SSeparator.h"
#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SArcModifierTableRow"

namespace ArcModifierTableRowImpl
{
	static const FLinearColor DropIndicatorColor(0.0f, 0.5f, 1.0f, 0.8f);
} // namespace ArcModifierTableRowImpl

void SArcModifierTableRow::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& OwnerTableView)
{
	Item = InArgs._Item;
	Record = InArgs._Record;
	TreeViewWeak = InArgs._TreeView;
	ColumnSchema = InArgs._ColumnSchema;
	TopLevelKind = InArgs._TopLevelKind;

	SMultiColumnTableRow<TSharedPtr<FArcModifierTreeItem>>::Construct(
		FSuperRowType::FArguments(),
		OwnerTableView);

	// Overlay: debug tint + drop separators on top of the row content.
	SetContent(
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			Content.Pin().ToSharedRef()
		]
		// Debug tint: green = eligible, red = ineligible, invisible when debug is off.
		+ SOverlay::Slot()
		[
			SNew(SColorBlock)
			.Color_Lambda([this]() -> FLinearColor
			{
				return GetDebugTintColor();
			})
			.Visibility_Lambda([this]() -> EVisibility
			{
				return IsDebugTintVisible() ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
			})
			.ToolTipText_Lambda([this]() -> FText
			{
				return GetDebugTooltipText();
			})
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Bottom)
		[
			SNew(SSeparator)
			.SeparatorImage(FAppStyle::GetBrush("PropertyEditor.HorizontalDottedLine"))
			.ColorAndOpacity_Lambda([this]() -> FSlateColor
			{
				return FSlateColor(ArcModifierTableRowImpl::DropIndicatorColor);
			})
			.Visibility_Lambda([this]() -> EVisibility
			{
				return bDragActive && !bDropAbove ? EVisibility::Visible : EVisibility::Hidden;
			})
		]
		+ SOverlay::Slot()
		.VAlign(VAlign_Top)
		[
			SNew(SSeparator)
			.SeparatorImage(FAppStyle::GetBrush("PropertyEditor.HorizontalDottedLine"))
			.ColorAndOpacity_Lambda([this]() -> FSlateColor
			{
				return FSlateColor(ArcModifierTableRowImpl::DropIndicatorColor);
			})
			.Visibility_Lambda([this]() -> EVisibility
			{
				return bDragActive && bDropAbove ? EVisibility::Visible : EVisibility::Hidden;
			})
		]
	);
}

TSharedRef<SWidget> SArcModifierTableRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	static const FName HandlesColumnId("Handles");

	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	// Handles column: expander + drag handle (drag handle only for top-level rows).
	if (ColumnName == HandlesColumnId)
	{
		const bool bIsTopLevel = (Item->Kind == TopLevelKind);
		const int32 RowKey = Item->ChildIndex;

		TSharedRef<SHorizontalBox> HandlesBox = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SExpanderArrow, SharedThis(this))
				.IndentAmount(12)
				.ShouldDrawWires(false)
			];

		if (bIsTopLevel)
		{
			HandlesBox->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SArcModifierRowHandle)
					.TreeView(TreeViewWeak)
					.RowIndex(RowKey)
				];
		}

		return HandlesBox;
	}

	// Data columns: delegate to the column schema's cell factory.
	if (ColumnSchema != nullptr)
	{
		for (const FArcModifierColumn& Col : *ColumnSchema)
		{
			if (Col.Id == ColumnName && Col.ForRowKind == Item->Kind && Col.CellFactory)
			{
				UArcJsonRecordEditObject* RecordPtr = Record.Get();
				if (RecordPtr != nullptr)
				{
					const int32 RowKey = Item->ChildIndex;
					return Col.CellFactory(RecordPtr, *Item, RowKey, TreeViewWeak);
				}
			}
		}
	}

	return SNullWidget::NullWidget;
}

void SArcModifierTableRow::OnDragEnter(
	const FGeometry& MyGeometry,
	const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FArcModifierRowDragDropOp> DragOp =
		DragDropEvent.GetOperationAs<FArcModifierRowDragDropOp>();
	if (DragOp.IsValid())
	{
		++DragActiveCounter;
		bDragActive = true;
		const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
		bDropAbove = LocalPos.Y < (MyGeometry.GetLocalSize().Y * 0.5f);
	}
}

void SArcModifierTableRow::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FArcModifierRowDragDropOp> DragOp =
		DragDropEvent.GetOperationAs<FArcModifierRowDragDropOp>();
	if (DragOp.IsValid())
	{
		--DragActiveCounter;
	}
}

FReply SArcModifierTableRow::OnDragOver(
	const FGeometry& MyGeometry,
	const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FArcModifierRowDragDropOp> DragOp =
		DragDropEvent.GetOperationAs<FArcModifierRowDragDropOp>();
	if (DragOp.IsValid())
	{
		const FVector2D LocalPos = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
		bDropAbove = LocalPos.Y < (MyGeometry.GetLocalSize().Y * 0.5f);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply SArcModifierTableRow::OnDrop(
	const FGeometry& MyGeometry,
	const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FArcModifierRowDragDropOp> DragOp =
		DragDropEvent.GetOperationAs<FArcModifierRowDragDropOp>();
	if (DragOp.IsValid() && Item.IsValid())
	{
		TSharedPtr<SArcModifierTreeView> TreeView = TreeViewWeak.Pin();
		if (TreeView.IsValid())
		{
			const int32 TargetIndex = bDropAbove ? Item->ChildIndex : (Item->ChildIndex + 1);
			TreeView->MoveRow(DragOp->SourceIndex, TargetIndex);
		}
		bDragActive = false;
		DragActiveCounter = 0;
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SArcModifierTableRow::Tick(
	const FGeometry& AllottedGeometry,
	const double InCurrentTime,
	const float InDeltaTime)
{
	SMultiColumnTableRow<TSharedPtr<FArcModifierTreeItem>>::Tick(
		AllottedGeometry, InCurrentTime, InDeltaTime);

	// Decay the drag-active indicator when no drag events are arriving.
	if (DragActiveCounter <= 0)
	{
		bDragActive = false;
		DragActiveCounter = 0;
	}
}

// ---------------------------------------------------------------------------
// Debug tint helpers
// ---------------------------------------------------------------------------

namespace ArcModifierTableRowDebugImpl
{
	/** Compute the evaluation map key matching the evaluator's encoding. */
	static int32 RowKey(const FArcModifierTreeItem& InItem)
	{
		// Top-level rows (ParentIndex == INDEX_NONE): keyed by ChildIndex alone.
		// Nested child rows: keyed as ParentIndex * 10000 + ChildIndex.
		if (InItem.ParentIndex == INDEX_NONE)
		{
			return InItem.ChildIndex;
		}
		return InItem.ParentIndex * 10000 + InItem.ChildIndex;
	}
} // namespace ArcModifierTableRowDebugImpl

bool SArcModifierTableRow::IsDebugTintVisible() const
{
	TSharedPtr<SArcModifierTreeView> TreeView = TreeViewWeak.Pin();
	if (!TreeView.IsValid() || !Item.IsValid())
	{
		return false;
	}
	const FArcModifierDebugEvaluation& Eval = TreeView->GetCurrentEvaluation();
	const int32 Key = ArcModifierTableRowDebugImpl::RowKey(*Item);
	return Eval.ByRowKey.Contains(Key);
}

FLinearColor SArcModifierTableRow::GetDebugTintColor() const
{
	TSharedPtr<SArcModifierTreeView> TreeView = TreeViewWeak.Pin();
	if (!TreeView.IsValid() || !Item.IsValid())
	{
		return FLinearColor::Transparent;
	}
	const FArcModifierDebugEvaluation& Eval = TreeView->GetCurrentEvaluation();
	const int32 Key = ArcModifierTableRowDebugImpl::RowKey(*Item);
	const FArcModifierRowEvaluation* RowEval = Eval.ByRowKey.Find(Key);
	if (RowEval == nullptr)
	{
		return FLinearColor::Transparent;
	}
	if (RowEval->bModifierEligible)
	{
		// Green tint: eligible.
		return FLinearColor(0.0f, 0.6f, 0.0f, 0.18f);
	}
	// Red tint: ineligible.
	return FLinearColor(0.7f, 0.0f, 0.0f, 0.18f);
}

FText SArcModifierTableRow::GetDebugTooltipText() const
{
	TSharedPtr<SArcModifierTreeView> TreeView = TreeViewWeak.Pin();
	if (!TreeView.IsValid() || !Item.IsValid())
	{
		return FText::GetEmpty();
	}
	const FArcModifierDebugEvaluation& Eval = TreeView->GetCurrentEvaluation();
	const int32 Key = ArcModifierTableRowDebugImpl::RowKey(*Item);
	const FArcModifierRowEvaluation* RowEval = Eval.ByRowKey.Find(Key);
	if (RowEval == nullptr)
	{
		return FText::GetEmpty();
	}
	if (RowEval->bModifierEligible)
	{
		return LOCTEXT("RowEligible", "Eligible");
	}
	return RowEval->FailureReason.IsEmpty()
		? LOCTEXT("RowIneligible", "Ineligible")
		: RowEval->FailureReason;
}

#undef LOCTEXT_NAMESPACE
