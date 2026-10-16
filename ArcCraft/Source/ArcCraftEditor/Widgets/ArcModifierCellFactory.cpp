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

#include "Widgets/ArcModifierCellFactory.h"

#include "Widgets/SNullWidget.h"

namespace ArcModifierCellFactoryImpl
{

} // namespace ArcModifierCellFactoryImpl

void FArcModifierCellFactory::RegisterBuiltins()
{
	// Built-in cell builders are registered by the per-asset column schema files
	// (e.g. ArcStaticModifierSet_ColumnSchema.cpp) via Register().
	// Nothing to pre-register globally here — each asset's schema injects its own builders.
}

void FArcModifierCellFactory::Register(FName ColumnId, EArcModifierRowKind RowKind, FCellBuilderFn Builder)
{
	TPair<EArcModifierRowKind, FName> Key(RowKind, ColumnId);
	Registry.Add(Key, MoveTemp(Builder));
}

TSharedRef<SWidget> FArcModifierCellFactory::MakeCell(
	FName ColumnId,
	EArcModifierRowKind RowKind,
	UArcJsonRecordEditObject* Record,
	FArcModifierTreeItem& Item,
	int32 RowKey) const
{
	TPair<EArcModifierRowKind, FName> Key(RowKind, ColumnId);
	const FCellBuilderFn* Builder = Registry.Find(Key);
	if (Builder != nullptr)
	{
		return (*Builder)(Record, Item, RowKey);
	}
	return SNullWidget::NullWidget;
}
