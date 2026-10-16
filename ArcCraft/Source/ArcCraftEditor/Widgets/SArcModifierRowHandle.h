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
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/ArcModifierTreeItem.h"

class SArcModifierTreeView;

/**
 * Drag-drop operation payload for modifier tree rows.
 * No transaction is opened here — the toolkit's HandleReorderTopLevel owns the
 * FScopedTransaction. This keeps undo to exactly one entry per drag-drop.
 */
class FArcModifierRowDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FArcModifierRowDragDropOp, FDecoratedDragDropOp)

	/** Index of the row that was dragged (within the top-level array). */
	int32 SourceIndex = INDEX_NONE;

	/** Weak pointer back to the tree view for refresh. */
	TWeakPtr<SArcModifierTreeView> TreeViewWeak;

	static TSharedRef<FArcModifierRowDragDropOp> New(
		TWeakPtr<SArcModifierTreeView> InTreeView,
		int32 InSourceIndex);
};

/**
 * Drag handle widget placed in the "Handles" column of each data row.
 * On drag-detected, starts FArcModifierRowDragDropOp.
 */
class SArcModifierRowHandle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcModifierRowHandle) {}
		SLATE_ARGUMENT(TWeakPtr<SArcModifierTreeView>, TreeView)
		SLATE_ARGUMENT(int32, RowIndex)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TWeakPtr<SArcModifierTreeView> TreeViewWeak;
	int32 RowIndex = INDEX_NONE;
};
