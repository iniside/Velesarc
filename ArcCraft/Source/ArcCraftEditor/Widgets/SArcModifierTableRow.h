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

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/ArcModifierTreeItem.h"
#include "Widgets/ArcModifierDebugContext.h"

class SArcModifierTreeView;
class UArcJsonRecordEditObject;
class FArcModifierCellFactory;
struct FArcModifierColumn;

/**
 * One row in SArcModifierTreeView.
 * GenerateWidgetForColumn dispatches to FArcModifierCellFactory for data cells,
 * and places SExpanderArrow + SArcModifierRowHandle for the "Handles" column.
 * The row also implements drag-drop accepts and drop-indicator rendering.
 */
class SArcModifierTableRow : public SMultiColumnTableRow<TSharedPtr<FArcModifierTreeItem>>
{
public:
	SLATE_BEGIN_ARGS(SArcModifierTableRow) {}
		SLATE_ARGUMENT(TSharedPtr<FArcModifierTreeItem>, Item)
		SLATE_ARGUMENT(TWeakObjectPtr<UArcJsonRecordEditObject>, Record)
		SLATE_ARGUMENT(TWeakPtr<SArcModifierTreeView>, TreeView)
		SLATE_ARGUMENT(const TArray<FArcModifierColumn>*, ColumnSchema)
		/** Row kind that is draggable at the top level. Drag handle is suppressed for all other kinds. */
		SLATE_ARGUMENT(EArcModifierRowKind, TopLevelKind)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TSharedPtr<FArcModifierTreeItem> Item;
	TWeakObjectPtr<UArcJsonRecordEditObject> Record;
	TWeakPtr<SArcModifierTreeView> TreeViewWeak;
	const TArray<FArcModifierColumn>* ColumnSchema = nullptr;
	EArcModifierRowKind TopLevelKind = EArcModifierRowKind::Modifier;

	/** Tint overlay decay for drag-active indicator (Chooser DragActiveCounter pattern). */
	int32 DragActiveCounter = 0;
	bool bDragActive = false;
	bool bDropAbove = false;

	// Debug tint helpers — read evaluation from the owning SArcModifierTreeView.
	bool IsDebugTintVisible() const;
	FLinearColor GetDebugTintColor() const;
	FText GetDebugTooltipText() const;
};
