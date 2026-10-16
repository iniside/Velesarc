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

#include "Static/ArcStaticModifierSet_ColumnSchema.h"

#include "ArcCraft/Static/ArcStaticModifierSetData.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "Widgets/ArcModifierTreeItem.h"
#include "Widgets/ArcModifierColumnSchema.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "Widgets/ArcModifierSharedCells.h"
#include "Widgets/SArcModifierTreeView.h"
#include "Shared/ArcCraftRecordToolkit.h"

#include "Modules/ModuleManager.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"
#include "GameplayTagContainer.h"

#define LOCTEXT_NAMESPACE "ArcStaticModifierSet_ColumnSchema"

namespace ArcStaticModifierSet_ColumnSchema_Impl
{

static FInstancedStruct* FindModifierStruct(UArcJsonRecordEditObject* Record, int32 Index)
{
	FArcStaticModifierSetData* ModifierSet = ArcCraftRecordToolkit::GetData<FArcStaticModifierSetData>(Record);
	if (ModifierSet == nullptr || !ModifierSet->Modifiers.IsValidIndex(Index))
	{
		return nullptr;
	}
	return &ModifierSet->Modifiers[Index];
}

static FArcCraftModifier* FindModifier(UArcJsonRecordEditObject* Record, int32 Index)
{
	FInstancedStruct* ModStruct = FindModifierStruct(Record, Index);
	return ModStruct != nullptr ? ModStruct->GetMutablePtr<FArcCraftModifier>() : nullptr;
}

/** Build a "Type" cell: read-only display of the current modifier subtype name. */
static TSharedRef<SWidget> MakeTypeCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindModifierStruct(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 ItemChildIndex = Item.ChildIndex;
	return SNew(STextBlock)
		.Text_Lambda([RecordWeak, ItemChildIndex]() -> FText
		{
			const FInstancedStruct* ModStruct = FindModifierStruct(RecordWeak.Get(), ItemChildIndex);
			const UScriptStruct* Struct = ModStruct != nullptr ? ModStruct->GetScriptStruct() : nullptr;
			return Struct ? Struct->GetDisplayNameText() : LOCTEXT("NoneType", "(none)");
		})
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

/** Build a "MinR" cell: float numeric entry box. */
static TSharedRef<SWidget> MakeMinRCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindModifierStruct(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 ChildIdx = Item.ChildIndex;
	return SNew(SNumericEntryBox<float>)
		.Value_Lambda([RecordWeak, ChildIdx]() -> TOptional<float>
		{
			const FArcCraftModifier* Mod = FindModifier(RecordWeak.Get(), ChildIdx);
			return Mod ? TOptional<float>(Mod->MinR) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([RecordWeak, ChildIdx](float NewVal, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcCraftModifier* Mod = FindModifier(EditRecord, ChildIdx);
			if (Mod == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetMinR", "Set Min R"), [Mod, NewVal]()
			{
				Mod->MinR = FMath::Max(0.0f, NewVal);
			});
		})
		.MinValue(0.0f)
		.MaxValue(2.0f)
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static ArcModifierSharedCells::FModifierLocator MakeModifierLocator(int32 Index)
{
	return [Index](UArcJsonRecordEditObject* Record) -> FInstancedStruct*
	{
		return FindModifierStruct(Record, Index);
	};
}

/** Build a "StatId" cell: editable for Craft Stat Modifier rows only. */
static TSharedRef<SWidget> MakeStatIdCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	return ArcModifierSharedCells::MakeStatIdCell(Record, MakeModifierLocator(Item.ChildIndex));
}

/** Build a "SlotTags" cell: chair tag container. */
static TSharedRef<SWidget> MakeSlotTagsCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	return ArcModifierSharedCells::MakeSlotTagsCell(Record, MakeModifierLocator(Item.ChildIndex));
}

/** Build a "Band" cell: read-only debug chip. Flat StaticModifierSet rows use ChildIndex as row key. */
static TSharedRef<SWidget> MakeBandCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> TreeViewWeak)
{
	if (FindModifierStruct(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}
	return ArcModifierSharedCells::MakeBandCell(TreeViewWeak, Item.ChildIndex);
}

} // namespace ArcStaticModifierSet_ColumnSchema_Impl

TArray<FArcModifierColumn> GetStaticModifierSetColumnSchema()
{
	using namespace ArcStaticModifierSet_ColumnSchema_Impl;

	TArray<FArcModifierColumn> Schema;

	// Type column
	{
		FArcModifierColumn Col;
		Col.Id = FName("Type");
		Col.Label = LOCTEXT("TypeCol", "Type");
		Col.Tooltip = LOCTEXT("TypeColTip", "Modifier struct type");
		Col.DefaultWidth = 140.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeTypeCell;
		Schema.Add(MoveTemp(Col));
	}

	// StatId
	{
		FArcModifierColumn Col;
		Col.Id = FName("StatId");
		Col.Label = LOCTEXT("StatIdCol", "Stat Id");
		Col.Tooltip = LOCTEXT("StatIdColTip", "Stat id in the craft rules stat catalog (Craft Stat Modifier only)");
		Col.DefaultWidth = 120.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeStatIdCell;
		Schema.Add(MoveTemp(Col));
	}

	// SlotTags
	{
		FArcModifierColumn Col;
		Col.Id = FName("SlotTags");
		Col.Label = LOCTEXT("SlotTagsCol", "Slot Tags");
		Col.Tooltip = LOCTEXT("SlotTagsColTip", "Chair tags the modifier results compete for (Modifier.Stat, Modifier.Passive, Modifier.Penalty); empty = unslotted");
		Col.DefaultWidth = 160.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeSlotTagsCell;
		Schema.Add(MoveTemp(Col));
	}

	// MinR column
	{
		FArcModifierColumn Col;
		Col.Id = FName("MinR");
		Col.Label = LOCTEXT("MinQCol", "Min R");
		Col.Tooltip = LOCTEXT("MinQColTip", "Minimum R threshold for this modifier to be eligible");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeMinRCell;
		Schema.Add(MoveTemp(Col));
	}

	// Band
	{
		FArcModifierColumn Col;
		Col.Id = FName("Band");
		Col.Label = LOCTEXT("BandCol", "Band");
		Col.Tooltip = LOCTEXT("BandColTip", "Debug: catalog range of a stat modifier in the debug output rarity band, ComputeBandValue at U=0 and U=1");
		Col.DefaultWidth = 90.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeBandCell;
		Schema.Add(MoveTemp(Col));
	}

	return Schema;
}

#undef LOCTEXT_NAMESPACE
