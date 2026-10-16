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
#include "Widgets/ArcModifierTreeItem.h"
#include "Widgets/ArcModifierDebugContext.h"

class SWidget;
class UArcJsonRecordEditObject;
class SArcModifierTreeView;
struct FArcModifierTreeItem;
struct FArcModifierDebugContext;

/**
 * Descriptor for one column in SArcModifierTreeView.
 * The cell and header factories are set by each asset's column schema file.
 */
struct FArcModifierColumn
{
	/** Unique identifier matching the SHeaderRow column id. */
	FName Id;

	/** Display label shown in the header. */
	FText Label;

	/** Tooltip shown when hovering the header. */
	FText Tooltip;

	/** Default column width in pixels. */
	float DefaultWidth = 80.0f;

	/** Which row kind this column produces a cell for.
	 *  For columns that render on all kinds, set to EArcModifierRowKind::Modifier
	 *  and branch inside the factory if needed. */
	EArcModifierRowKind ForRowKind = EArcModifierRowKind::Modifier;

	/** Factory that produces a cell widget for a data row.
	 *  @param Record        The record edit object being edited.
	 *  @param Item          The row item being rendered.
	 *  @param RowKey        Encoded row key (same encoding as FArcModifierDebugEvaluation::ByRowKey).
	 *  @param TreeViewWeak  Weak pointer to the owning SArcModifierTreeView, for debug eval access. */
	TFunction<TSharedRef<SWidget>(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey, TWeakPtr<SArcModifierTreeView> TreeViewWeak)> CellFactory;

	/** Factory that produces a header widget.
	 *  May return SNullWidget::NullWidget for simple text columns (handled by SArcModifierTreeView). */
	TFunction<TSharedRef<SWidget>(UArcJsonRecordEditObject* Record)> HeaderFactory;
};
