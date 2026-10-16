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

class SWidget;
class UArcJsonRecordEditObject;
struct FArcModifierTreeItem;

/**
 * Module-scope cell factory registry.
 * Owned by FArcCraftEditorModule — created in StartupModule, destroyed in ShutdownModule.
 * All cell builders for the three modifier-bearing records are pre-registered here.
 */
class FArcModifierCellFactory
{
public:
	using FCellBuilderFn = TFunction<TSharedRef<SWidget>(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey)>;

	/** Register the built-in cell builders for all supported column / row-kind combinations. */
	void RegisterBuiltins();

	/** Look up and invoke the cell builder for the given column id and row kind.
	 *  Returns SNullWidget::NullWidget if no builder is registered. */
	TSharedRef<SWidget> MakeCell(
		FName ColumnId,
		EArcModifierRowKind RowKind,
		UArcJsonRecordEditObject* Record,
		FArcModifierTreeItem& Item,
		int32 RowKey) const;

	/** Register a custom cell builder, overriding any existing registration. */
	void Register(FName ColumnId, EArcModifierRowKind RowKind, FCellBuilderFn Builder);

private:
	TMap<TPair<EArcModifierRowKind, FName>, FCellBuilderFn> Registry;
};
