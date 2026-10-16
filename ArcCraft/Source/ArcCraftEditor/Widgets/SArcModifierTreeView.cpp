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

#include "Widgets/SArcModifierTreeView.h"
#include "Widgets/SArcModifierTableRow.h"
#include "Widgets/ArcModifierColumnSchema.h"

#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/AppStyle.h"


#define LOCTEXT_NAMESPACE "SArcModifierTreeView"

namespace ArcModifierTreeViewImpl
{
	static const FName HandlesColumnId("Handles");
	static const float HandlesColumnWidth = 36.0f;
} // namespace ArcModifierTreeViewImpl

void SArcModifierTreeView::Construct(const FArguments& InArgs)
{
	RecordWeak = InArgs._Record;
	ColumnSchema = InArgs._ColumnSchema;
	AddTopLevelLabel = InArgs._AddTopLevelLabel;
	TopLevelKind = InArgs._TopLevelKind;
	OnBuildRowsDelegate = InArgs._OnBuildRows;
	OnEvaluateDebugDelegate = InArgs._OnEvaluateDebug;
	OnRowSelectedDelegate = InArgs._OnRowSelected;
	OnAddTopLevelDelegate = InArgs._OnAddTopLevel;
	OnAddChildOfSelectedDelegate = InArgs._OnAddChildOfSelected;
	OnDuplicateRowDelegate = InArgs._OnDuplicateRow;
	OnDeleteRowDelegate = InArgs._OnDeleteRow;
	OnReorderTopLevelDelegate = InArgs._OnReorderTopLevel;
	OnEvaluationChangedDelegate = InArgs._OnEvaluationChanged;

	// Build initial header row.
	HeaderRow = SNew(SHeaderRow);

	HeaderRow->AddColumn(
		SHeaderRow::Column(ArcModifierTreeViewImpl::HandlesColumnId)
		.DefaultLabel(FText::GetEmpty())
		.ManualWidth(ArcModifierTreeViewImpl::HandlesColumnWidth));

	for (const FArcModifierColumn& Col : ColumnSchema)
	{
		SHeaderRow::FColumn::FArguments ColArgs;
		ColArgs.ColumnId(Col.Id);
		ColArgs.ManualWidth(Col.DefaultWidth);
		ColArgs.DefaultLabel(Col.Label);
		if (!Col.Tooltip.IsEmpty())
		{
			ColArgs.DefaultTooltip(Col.Tooltip);
		}
		HeaderRow->AddColumn(ColArgs);
	}

	// Build the tree view widget.
	SAssignNew(TreeView, STreeView<TSharedPtr<FArcModifierTreeItem>>)
		.TreeItemsSource(&RootItems)
		.OnGenerateRow(this, &SArcModifierTreeView::OnGenerateRow)
		.OnGetChildren(this, &SArcModifierTreeView::OnGetChildren)
		.OnSelectionChanged(this, &SArcModifierTreeView::OnSelectionChanged)
		.SelectionMode(ESelectionMode::Single)
		.HeaderRow(HeaderRow);

	// Initial row population.
	RebuildAll();

	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(4.0f, 2.0f))
			[
				SNew(SHorizontalBox)

				// Primary "+ Add" button — label injected by toolkit, always enabled.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text_Lambda([this]() -> FText
					{
						return AddTopLevelLabel.IsEmpty()
							? LOCTEXT("AddBtn", "+ Add")
							: AddTopLevelLabel;
					})
					.ToolTipText(LOCTEXT("AddBtnTip", "Add a new top-level row"))
					.OnClicked_Lambda([this]() -> FReply
					{
						OnAddTopLevelDelegate.ExecuteIfBound(SelectedItem);
						return FReply::Handled();
					})
				]

				// "+ Add Child" button — visible only when OnAddChildOfSelected is bound.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("AddChildBtn", "+ Add Modifier"))
					.ToolTipText(LOCTEXT("AddChildBtnTip", "Add a modifier under the selected rule or entry"))
					.Visibility_Lambda([this]() -> EVisibility
					{
						return OnAddChildOfSelectedDelegate.IsBound()
							? EVisibility::Visible
							: EVisibility::Collapsed;
					})
					.IsEnabled_Lambda([this]() -> bool
					{
						if (!SelectedItem.IsValid()) { return false; }
						return SelectedItem->Kind == EArcModifierRowKind::Rule
							|| SelectedItem->Kind == EArcModifierRowKind::Entry;
					})
					.OnClicked_Lambda([this]() -> FReply
					{
						OnAddChildOfSelectedDelegate.ExecuteIfBound(SelectedItem);
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("DupBtn", "Duplicate"))
					.ToolTipText(LOCTEXT("DupBtnTip", "Duplicate the selected row"))
					.IsEnabled_Lambda([this]() -> bool { return SelectedItem.IsValid(); })
					.OnClicked_Lambda([this]() -> FReply
					{
						OnDuplicateRowDelegate.ExecuteIfBound(SelectedItem);
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("DelBtn", "Delete"))
					.ToolTipText(LOCTEXT("DelBtnTip", "Delete the selected row"))
					.IsEnabled_Lambda([this]() -> bool { return SelectedItem.IsValid(); })
					.OnClicked_Lambda([this]() -> FReply
					{
						OnDeleteRowDelegate.ExecuteIfBound(SelectedItem);
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("UpBtn", "Move Up"))
					.ToolTipText(LOCTEXT("UpBtnTip", "Move the selected row up"))
					.IsEnabled_Lambda([this]() -> bool
					{
						return SelectedItem.IsValid()
							&& SelectedItem->Kind == TopLevelKind
							&& SelectedItem->ChildIndex > 0;
					})
					.OnClicked_Lambda([this]() -> FReply
					{
						if (!SelectedItem.IsValid()) { return FReply::Handled(); }
						const int32 Src = SelectedItem->ChildIndex;
						if (Src > 0)
						{
							OnReorderTopLevelDelegate.ExecuteIfBound(Src, Src - 1);
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("DownBtn", "Move Down"))
					.ToolTipText(LOCTEXT("DownBtnTip", "Move the selected row down"))
					.IsEnabled_Lambda([this]() -> bool
					{
						if (!SelectedItem.IsValid()) { return false; }
						if (SelectedItem->Kind != TopLevelKind) { return false; }
						const int32 LastIdx = RootItems.Num() - 1;
						return SelectedItem->ChildIndex < LastIdx;
					})
					.OnClicked_Lambda([this]() -> FReply
					{
						if (!SelectedItem.IsValid()) { return FReply::Handled(); }
						const int32 Src = SelectedItem->ChildIndex;
						const int32 LastIdx = RootItems.Num() - 1;
						if (Src < LastIdx)
						{
							OnReorderTopLevelDelegate.ExecuteIfBound(Src, Src + 1);
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ExpandBtn", "Expand All"))
					.OnClicked(this, &SArcModifierTreeView::OnExpandAllClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CollapseBtn", "Collapse All"))
					.OnClicked(this, &SArcModifierTreeView::OnCollapseAllClicked)
				]
			]
		]

		// Tree view
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			TreeView.ToSharedRef()
		]
	];
}

void SArcModifierTreeView::RebuildAll()
{
	// --- Step 1: snapshot stable keys BEFORE nulling SelectedItem ---

	// Snapshot the selected item's stable key while SelectedItem is still valid.
	TOptional<TTuple<EArcModifierRowKind, int32, int32>> SelectedKey;
	if (SelectedItem.IsValid())
	{
		SelectedKey = MakeTuple(SelectedItem->Kind, SelectedItem->ParentIndex, SelectedItem->ChildIndex);
	}

	// Snapshot expansion keys by walking the current RootItems + their Children.
	TSet<TTuple<EArcModifierRowKind, int32, int32>> ExpandedKeys;
	if (TreeView.IsValid())
	{
		for (const TSharedPtr<FArcModifierTreeItem>& RootItem : RootItems)
		{
			if (RootItem.IsValid() && TreeView->IsItemExpanded(RootItem))
			{
				ExpandedKeys.Add(MakeTuple(RootItem->Kind, RootItem->ParentIndex, RootItem->ChildIndex));
			}
			if (RootItem.IsValid())
			{
				for (const TSharedPtr<FArcModifierTreeItem>& Child : RootItem->Children)
				{
					if (Child.IsValid() && TreeView->IsItemExpanded(Child))
					{
						ExpandedKeys.Add(MakeTuple(Child->Kind, Child->ParentIndex, Child->ChildIndex));
					}
				}
			}
		}
	}

	// Null the selected item before discarding RootItems to avoid dangling TSharedPtr.
	SelectedItem = nullptr;

	// --- Step 2: rebuild rows ---

	if (OnBuildRowsDelegate.IsBound())
	{
		RootItems = OnBuildRowsDelegate.Execute(RecordWeak.Get(), DebugContext);
	}
	else
	{
		RootItems.Reset();
	}

	// --- Step 3: restore expansion and selection on new items ---

	if (TreeView.IsValid())
	{
		// Restore expansion state.
		for (const TSharedPtr<FArcModifierTreeItem>& RootItem : RootItems)
		{
			if (!RootItem.IsValid()) { continue; }

			const TTuple<EArcModifierRowKind, int32, int32> RootKey =
				MakeTuple(RootItem->Kind, RootItem->ParentIndex, RootItem->ChildIndex);
			if (ExpandedKeys.Contains(RootKey))
			{
				TreeView->SetItemExpansion(RootItem, true);
			}

			for (const TSharedPtr<FArcModifierTreeItem>& Child : RootItem->Children)
			{
				if (!Child.IsValid()) { continue; }

				const TTuple<EArcModifierRowKind, int32, int32> ChildKey =
					MakeTuple(Child->Kind, Child->ParentIndex, Child->ChildIndex);
				if (ExpandedKeys.Contains(ChildKey))
				{
					TreeView->SetItemExpansion(Child, true);
				}
			}
		}

		// Restore selection.
		if (SelectedKey.IsSet())
		{
			const TTuple<EArcModifierRowKind, int32, int32> WantedKey = SelectedKey.GetValue();
			TSharedPtr<FArcModifierTreeItem> RestoredItem;

			for (const TSharedPtr<FArcModifierTreeItem>& RootItem : RootItems)
			{
				if (!RootItem.IsValid()) { continue; }

				const TTuple<EArcModifierRowKind, int32, int32> RootKey =
					MakeTuple(RootItem->Kind, RootItem->ParentIndex, RootItem->ChildIndex);
				if (RootKey == WantedKey)
				{
					RestoredItem = RootItem;
					break;
				}

				for (const TSharedPtr<FArcModifierTreeItem>& Child : RootItem->Children)
				{
					if (!Child.IsValid()) { continue; }

					const TTuple<EArcModifierRowKind, int32, int32> ChildKey =
						MakeTuple(Child->Kind, Child->ParentIndex, Child->ChildIndex);
					if (ChildKey == WantedKey)
					{
						RestoredItem = Child;
						break;
					}
				}

				if (RestoredItem.IsValid()) { break; }
			}

			if (RestoredItem.IsValid())
			{
				SelectedItem = RestoredItem;
				TreeView->SetItemSelection(RestoredItem, true, ESelectInfo::Direct);
				OnRowSelectedDelegate.ExecuteIfBound(RestoredItem);
			}
			else
			{
				// Item no longer exists after rebuild — clear details panel.
				OnRowSelectedDelegate.ExecuteIfBound(nullptr);
			}
		}

		TreeView->RequestTreeRefresh();
	}
}

void SArcModifierTreeView::RefreshRows()
{
	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

void SArcModifierTreeView::SetDebugContext(const FArcModifierDebugContext& NewContext)
{
	DebugContext = NewContext;

	// Re-evaluate when the evaluator delegate is bound and debug preview is active.
	if (OnEvaluateDebugDelegate.IsBound() && DebugContext.bEnabled)
	{
		UArcJsonRecordEditObject* Record = RecordWeak.Get();
		if (Record != nullptr)
		{
			CurrentEvaluation = OnEvaluateDebugDelegate.Execute(Record, DebugContext);
		}
		else
		{
			CurrentEvaluation = FArcModifierDebugEvaluation();
		}
	}
	else
	{
		CurrentEvaluation = FArcModifierDebugEvaluation();
	}

	OnEvaluationChangedDelegate.ExecuteIfBound(CurrentEvaluation);

	RefreshRows();
}

void SArcModifierTreeView::SelectByKey(EArcModifierRowKind Kind, int32 ParentIdx, int32 ChildIdx)
{
	if (!TreeView.IsValid()) { return; }

	const TTuple<EArcModifierRowKind, int32, int32> WantedKey = MakeTuple(Kind, ParentIdx, ChildIdx);
	TSharedPtr<FArcModifierTreeItem> FoundItem;

	for (const TSharedPtr<FArcModifierTreeItem>& RootItem : RootItems)
	{
		if (!RootItem.IsValid()) { continue; }

		const TTuple<EArcModifierRowKind, int32, int32> RootKey =
			MakeTuple(RootItem->Kind, RootItem->ParentIndex, RootItem->ChildIndex);
		if (RootKey == WantedKey)
		{
			FoundItem = RootItem;
			break;
		}

		for (const TSharedPtr<FArcModifierTreeItem>& Child : RootItem->Children)
		{
			if (!Child.IsValid()) { continue; }

			const TTuple<EArcModifierRowKind, int32, int32> ChildKey =
				MakeTuple(Child->Kind, Child->ParentIndex, Child->ChildIndex);
			if (ChildKey == WantedKey)
			{
				// Ensure the parent row is expanded so the child is visible.
				TreeView->SetItemExpansion(RootItem, true);
				FoundItem = Child;
				break;
			}
		}

		if (FoundItem.IsValid()) { break; }
	}

	if (FoundItem.IsValid())
	{
		SelectedItem = FoundItem;
		TreeView->SetItemSelection(FoundItem, true, ESelectInfo::Direct);
		OnRowSelectedDelegate.ExecuteIfBound(FoundItem);
	}
}

void SArcModifierTreeView::MoveRow(int32 SourceIndex, int32 TargetIndex)
{
	if (!RootItems.IsValidIndex(SourceIndex)) { return; }

	const int32 FinalTarget = FMath::Clamp(TargetIndex, 0, RootItems.Num());
	if (FinalTarget == SourceIndex || FinalTarget == SourceIndex + 1) { return; }

	// Route to the toolkit's reorder handler, which owns the actual asset mutation.
	// The handler also calls RebuildAll, so we don't touch RootItems here (the old
	// UI-only swap was a footgun: it left RootItems out of sync with the asset and
	// any FStructOnScope held by a row details view could dereference stale memory).
	OnReorderTopLevelDelegate.ExecuteIfBound(SourceIndex, FinalTarget);
}

// ---------------------------------------------------------------------------
// STreeView callbacks
// ---------------------------------------------------------------------------

TSharedRef<ITableRow> SArcModifierTreeView::OnGenerateRow(
	TSharedPtr<FArcModifierTreeItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SArcModifierTableRow, OwnerTable)
		.Item(Item)
		.Record(RecordWeak)
		.TreeView(SharedThis(this))
		.ColumnSchema(&ColumnSchema)
		.TopLevelKind(TopLevelKind);
}

void SArcModifierTreeView::OnGetChildren(
	TSharedPtr<FArcModifierTreeItem> Item,
	TArray<TSharedPtr<FArcModifierTreeItem>>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SArcModifierTreeView::OnSelectionChanged(
	TSharedPtr<FArcModifierTreeItem> Item,
	ESelectInfo::Type SelectType)
{
	SelectedItem = Item;
	OnRowSelectedDelegate.ExecuteIfBound(Item);
}

// ---------------------------------------------------------------------------
// Toolbar handlers
// ---------------------------------------------------------------------------

FReply SArcModifierTreeView::OnExpandAllClicked()
{
	if (TreeView.IsValid())
	{
		for (const TSharedPtr<FArcModifierTreeItem>& RootItem : RootItems)
		{
			TreeView->SetItemExpansion(RootItem, true);
			for (const TSharedPtr<FArcModifierTreeItem>& Child : RootItem->Children)
			{
				TreeView->SetItemExpansion(Child, true);
			}
		}
	}
	return FReply::Handled();
}

FReply SArcModifierTreeView::OnCollapseAllClicked()
{
	if (TreeView.IsValid())
	{
		for (const TSharedPtr<FArcModifierTreeItem>& RootItem : RootItems)
		{
			TreeView->SetItemExpansion(RootItem, false);
		}
	}
	return FReply::Handled();
}

void SArcModifierTreeView::RebuildHeaderRow()
{
	if (!HeaderRow.IsValid()) { return; }

	HeaderRow->ClearColumns();

	HeaderRow->AddColumn(
		SHeaderRow::Column(ArcModifierTreeViewImpl::HandlesColumnId)
		.DefaultLabel(FText::GetEmpty())
		.ManualWidth(ArcModifierTreeViewImpl::HandlesColumnWidth));

	for (const FArcModifierColumn& Col : ColumnSchema)
	{
		SHeaderRow::FColumn::FArguments ColArgs;
		ColArgs.ColumnId(Col.Id);
		ColArgs.ManualWidth(Col.DefaultWidth);
		ColArgs.DefaultLabel(Col.Label);
		if (!Col.Tooltip.IsEmpty())
		{
			ColArgs.DefaultTooltip(Col.Tooltip);
		}
		HeaderRow->AddColumn(ColArgs);
	}
}

#undef LOCTEXT_NAMESPACE
