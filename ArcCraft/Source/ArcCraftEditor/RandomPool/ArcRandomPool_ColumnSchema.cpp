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

#include "RandomPool/ArcRandomPool_ColumnSchema.h"

#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/Recipe/ArcRandomPoolEvaluation.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "Widgets/ArcModifierTreeItem.h"
#include "Widgets/ArcModifierColumnSchema.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "Widgets/ArcModifierSharedCells.h"
#include "Widgets/SArcModifierTreeView.h"
#include "Shared/ArcCraftRecordToolkit.h"

#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"
#include "GameplayTagContainer.h"

#define LOCTEXT_NAMESPACE "ArcRandomPool_ColumnSchema"

namespace ArcRandomPool_ColumnSchema_Impl
{

static FArcRandomPoolEntry* FindEntry(UArcJsonRecordEditObject* Record, int32 EntryIdx)
{
	FArcRandomPoolDefinitionData* PoolDef = ArcCraftRecordToolkit::GetData<FArcRandomPoolDefinitionData>(Record);
	if (PoolDef == nullptr || !PoolDef->Entries.IsValidIndex(EntryIdx))
	{
		return nullptr;
	}
	return &PoolDef->Entries[EntryIdx];
}

static FInstancedStruct* FindModifierStruct(UArcJsonRecordEditObject* Record, int32 EntryIdx, int32 ModIdx)
{
	FArcRandomPoolEntry* Entry = FindEntry(Record, EntryIdx);
	if (Entry == nullptr || !Entry->Modifiers.IsValidIndex(ModIdx))
	{
		return nullptr;
	}
	return &Entry->Modifiers[ModIdx];
}

static FArcCraftModifier* FindModifier(UArcJsonRecordEditObject* Record, int32 EntryIdx, int32 ModIdx)
{
	FInstancedStruct* ModStruct = FindModifierStruct(Record, EntryIdx, ModIdx);
	return ModStruct != nullptr ? ModStruct->GetMutablePtr<FArcCraftModifier>() : nullptr;
}

// ---------------------------------------------------------------------------
// Entry-row cell factories
// ---------------------------------------------------------------------------

static TSharedRef<SWidget> MakeEntryDisplayNameCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindEntry(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 EntryIdx = Item.ChildIndex;
	return SNew(SInlineEditableTextBlock)
		.Text_Lambda([RecordWeak, EntryIdx]() -> FText
		{
			const FArcRandomPoolEntry* Entry = FindEntry(RecordWeak.Get(), EntryIdx);
			return Entry != nullptr ? Entry->DisplayName : FText::GetEmpty();
		})
		.OnTextCommitted_Lambda([RecordWeak, EntryIdx](const FText& NewText, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcRandomPoolEntry* Entry = FindEntry(EditRecord, EntryIdx);
			if (Entry == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetEntryName", "Set Entry Name"), [Entry, &NewText]()
			{
				Entry->DisplayName = NewText;
			});
		})
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeEntryBaseWeightCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindEntry(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 EntryIdx = Item.ChildIndex;
	return SNew(SNumericEntryBox<float>)
		.Value_Lambda([RecordWeak, EntryIdx]() -> TOptional<float>
		{
			const FArcRandomPoolEntry* Entry = FindEntry(RecordWeak.Get(), EntryIdx);
			return Entry != nullptr ? TOptional<float>(Entry->BaseWeight) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([RecordWeak, EntryIdx](float NewVal, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcRandomPoolEntry* Entry = FindEntry(EditRecord, EntryIdx);
			if (Entry == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetBaseWeight", "Set Entry Base Weight"), [Entry, NewVal]()
			{
				Entry->BaseWeight = FMath::Max(0.001f, NewVal);
			});
		})
		.MinValue(0.001f)
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeEntryMinQCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindEntry(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 EntryIdx = Item.ChildIndex;
	return SNew(SNumericEntryBox<float>)
		.Value_Lambda([RecordWeak, EntryIdx]() -> TOptional<float>
		{
			const FArcRandomPoolEntry* Entry = FindEntry(RecordWeak.Get(), EntryIdx);
			return Entry != nullptr ? TOptional<float>(Entry->MinR) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([RecordWeak, EntryIdx](float NewVal, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcRandomPoolEntry* Entry = FindEntry(EditRecord, EntryIdx);
			if (Entry == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetEntryMinQ", "Set Entry Min R"), [Entry, NewVal]()
			{
				Entry->MinR = FMath::Max(0.0f, NewVal);
			});
		})
		.MinValue(0.0f)
		.MaxValue(2.0f)
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeEntryCostCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindEntry(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 EntryIdx = Item.ChildIndex;
	return SNew(SNumericEntryBox<float>)
		.Value_Lambda([RecordWeak, EntryIdx]() -> TOptional<float>
		{
			const FArcRandomPoolEntry* Entry = FindEntry(RecordWeak.Get(), EntryIdx);
			return Entry != nullptr ? TOptional<float>(Entry->Cost) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([RecordWeak, EntryIdx](float NewVal, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcRandomPoolEntry* Entry = FindEntry(EditRecord, EntryIdx);
			if (Entry == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetEntryCost", "Set Entry Cost"), [Entry, NewVal]()
			{
				Entry->Cost = FMath::Max(0.0f, NewVal);
			});
		})
		.MinValue(0.0f)
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeEntryEffectiveWeightCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> TreeViewWeak)
{
	return FindEntry(Record, Item.ChildIndex) != nullptr
		? ArcModifierSharedCells::MakeWeightCell(TreeViewWeak, Item.ChildIndex)
		: SNullWidget::NullWidget;
}

static TSharedRef<SWidget> MakeEntryMatchedTagsCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> TreeViewWeak)
{
	return FindEntry(Record, Item.ChildIndex) != nullptr
		? ArcModifierSharedCells::MakeMatchedTagsCell(TreeViewWeak, Item.ChildIndex)
		: SNullWidget::NullWidget;
}

static TSharedRef<SWidget> MakeEntryIngredientShareCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> TreeViewWeak)
{
	return FindEntry(Record, Item.ChildIndex) != nullptr
		? ArcModifierSharedCells::MakeIngredientShareCell(TreeViewWeak, Item.ChildIndex)
		: SNullWidget::NullWidget;
}

// ---------------------------------------------------------------------------
// Modifier child-row cell factories
// ---------------------------------------------------------------------------

static TSharedRef<SWidget> MakeModifierTypeCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindModifierStruct(Record, Item.ParentIndex, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 ParentIdx = Item.ParentIndex;
	const int32 ChildIdx = Item.ChildIndex;
	return SNew(STextBlock)
		.Text_Lambda([RecordWeak, ParentIdx, ChildIdx]() -> FText
		{
			const FInstancedStruct* ModStruct = FindModifierStruct(RecordWeak.Get(), ParentIdx, ChildIdx);
			const UScriptStruct* Struct = ModStruct != nullptr ? ModStruct->GetScriptStruct() : nullptr;
			return Struct
				? Struct->GetDisplayNameText()
				: LOCTEXT("NoneType", "(none)");
		})
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeModifierMinRCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindModifierStruct(Record, Item.ParentIndex, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 ParentIdx = Item.ParentIndex;
	const int32 ChildIdx = Item.ChildIndex;
	return SNew(SNumericEntryBox<float>)
		.Value_Lambda([RecordWeak, ParentIdx, ChildIdx]() -> TOptional<float>
		{
			const FArcCraftModifier* Mod = FindModifier(RecordWeak.Get(), ParentIdx, ChildIdx);
			return Mod ? TOptional<float>(Mod->MinR) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([RecordWeak, ParentIdx, ChildIdx](float NewVal, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcCraftModifier* Mod = FindModifier(EditRecord, ParentIdx, ChildIdx);
			if (Mod == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetModMinQ", "Set Modifier Min R"), [Mod, NewVal]()
			{
				Mod->MinR = FMath::Max(0.0f, NewVal);
			});
		})
		.MinValue(0.0f)
		.MaxValue(2.0f)
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static ArcModifierSharedCells::FModifierLocator MakeModifierLocator(int32 EntryIdx, int32 ModIdx)
{
	return [EntryIdx, ModIdx](UArcJsonRecordEditObject* Record) -> FInstancedStruct*
	{
		return FindModifierStruct(Record, EntryIdx, ModIdx);
	};
}

static TSharedRef<SWidget> MakeModifierStatIdCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	return ArcModifierSharedCells::MakeStatIdCell(Record, MakeModifierLocator(Item.ParentIndex, Item.ChildIndex));
}

static TSharedRef<SWidget> MakeModifierSlotTagsCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	return ArcModifierSharedCells::MakeSlotTagsCell(Record, MakeModifierLocator(Item.ParentIndex, Item.ChildIndex));
}

static TSharedRef<SWidget> MakeModifierBandCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> TreeViewWeak)
{
	if (FindModifierStruct(Record, Item.ParentIndex, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}
	return ArcModifierSharedCells::MakeBandCell(TreeViewWeak, Item.ParentIndex * 10000 + Item.ChildIndex);
}

} // namespace ArcRandomPool_ColumnSchema_Impl

// ---------------------------------------------------------------------------
// Public schema constructors
// ---------------------------------------------------------------------------

namespace ArcRandomPool_ColumnSchema
{

TArray<FArcModifierColumn> GetEntryRowSchema()
{
	using namespace ArcRandomPool_ColumnSchema_Impl;

	TArray<FArcModifierColumn> Schema;

	// DisplayName
	{
		FArcModifierColumn Col;
		Col.Id = FName("EntryName");
		Col.Label = LOCTEXT("EntryNameCol", "Name");
		Col.Tooltip = LOCTEXT("EntryNameColTip", "Editable name for this pool entry");
		Col.DefaultWidth = 120.0f;
		Col.ForRowKind = EArcModifierRowKind::Entry;
		Col.CellFactory = &MakeEntryDisplayNameCell;
		Schema.Add(MoveTemp(Col));
	}

	// BaseWeight
	{
		FArcModifierColumn Col;
		Col.Id = FName("EntryBaseWeight");
		Col.Label = LOCTEXT("EntryBaseWeightCol", "Base Weight");
		Col.Tooltip = LOCTEXT("EntryBaseWeightColTip", "Entry selection weight before R scaling");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Entry;
		Col.CellFactory = &MakeEntryBaseWeightCell;
		Schema.Add(MoveTemp(Col));
	}

	// MinQ (entry-level)
	{
		FArcModifierColumn Col;
		Col.Id = FName("EntryMinQ");
		Col.Label = LOCTEXT("EntryMinQCol", "Min R");
		Col.Tooltip = LOCTEXT("EntryMinQColTip", "Minimum R threshold for this entry to be eligible");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Entry;
		Col.CellFactory = &MakeEntryMinQCell;
		Schema.Add(MoveTemp(Col));
	}

	// Cost (budget mode)
	{
		FArcModifierColumn Col;
		Col.Id = FName("EntryCost");
		Col.Label = LOCTEXT("EntryCostCol", "Cost");
		Col.Tooltip = LOCTEXT("EntryCostColTip", "Resource cost in budget-mode selection");
		Col.DefaultWidth = 50.0f;
		Col.ForRowKind = EArcModifierRowKind::Entry;
		Col.CellFactory = &MakeEntryCostCell;
		Schema.Add(MoveTemp(Col));
	}

	// EffectiveWeight (debug chip)
	{
		FArcModifierColumn Col;
		Col.Id = FName("EntryEffWeight");
		Col.Label = LOCTEXT("EntryEffWeightCol", "Eff. Weight");
		Col.Tooltip = LOCTEXT("EntryEffWeightColTip", "Effective selection weight after R scaling");
		Col.DefaultWidth = 50.0f;
		Col.ForRowKind = EArcModifierRowKind::Entry;
		Col.CellFactory = &MakeEntryEffectiveWeightCell;
		Schema.Add(MoveTemp(Col));
	}

	// EntryMatchedTags
	{
		FArcModifierColumn Col;
		Col.Id = FName("EntryMatchedTags");
		Col.Label = LOCTEXT("EntryMatchedTagsCol", "Matched Tags");
		Col.Tooltip = LOCTEXT("EntryMatchedTagsColTip", "Debug: required ingredient tags of the entry that the debug ingredients hold");
		Col.DefaultWidth = 160.0f;
		Col.ForRowKind = EArcModifierRowKind::Entry;
		Col.CellFactory = &MakeEntryMatchedTagsCell;
		Schema.Add(MoveTemp(Col));
	}

	// EntryIngredientShare
	{
		FArcModifierColumn Col;
		Col.Id = FName("EntryIngredientShare");
		Col.Label = LOCTEXT("EntryIngredientShareCol", "Share");
		Col.Tooltip = LOCTEXT("EntryIngredientShareColTip", "Debug: share of the consumed amount in the slots holding the entry required tags");
		Col.DefaultWidth = 50.0f;
		Col.ForRowKind = EArcModifierRowKind::Entry;
		Col.CellFactory = &MakeEntryIngredientShareCell;
		Schema.Add(MoveTemp(Col));
	}

	return Schema;
}

TArray<FArcModifierColumn> GetModifierChildRowSchema()
{
	using namespace ArcRandomPool_ColumnSchema_Impl;

	TArray<FArcModifierColumn> Schema;

	// Type (FInstancedStruct subtype picker)
	{
		FArcModifierColumn Col;
		Col.Id = FName("ModType");
		Col.Label = LOCTEXT("ModTypeCol", "Type");
		Col.Tooltip = LOCTEXT("ModTypeColTip", "Modifier struct type");
		Col.DefaultWidth = 140.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeModifierTypeCell;
		Schema.Add(MoveTemp(Col));
	}

	// ModStatId
	{
		FArcModifierColumn Col;
		Col.Id = FName("ModStatId");
		Col.Label = LOCTEXT("ModStatIdCol", "Stat Id");
		Col.Tooltip = LOCTEXT("ModStatIdColTip", "Stat id in the craft rules stat catalog (Craft Stat Modifier only)");
		Col.DefaultWidth = 120.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeModifierStatIdCell;
		Schema.Add(MoveTemp(Col));
	}

	// ModSlotTags
	{
		FArcModifierColumn Col;
		Col.Id = FName("ModSlotTags");
		Col.Label = LOCTEXT("ModSlotTagsCol", "Slot Tags");
		Col.Tooltip = LOCTEXT("ModSlotTagsColTip", "Chair tags the modifier results compete for (Modifier.Stat, Modifier.Passive, Modifier.Penalty); empty = unslotted");
		Col.DefaultWidth = 160.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeModifierSlotTagsCell;
		Schema.Add(MoveTemp(Col));
	}

	// MinR (modifier threshold, checked against Ctx.AverageR)
	{
		FArcModifierColumn Col;
		Col.Id = FName("ModMinQ");
		Col.Label = LOCTEXT("ModMinQCol", "Min R");
		Col.Tooltip = LOCTEXT("ModMinQColTip", "Minimum R threshold for this modifier to be eligible");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeModifierMinRCell;
		Schema.Add(MoveTemp(Col));
	}

	// ModBand
	{
		FArcModifierColumn Col;
		Col.Id = FName("ModBand");
		Col.Label = LOCTEXT("ModBandCol", "Band");
		Col.Tooltip = LOCTEXT("ModBandColTip", "Debug: band edges of a stat modifier's catalog range for the debug output rarity (ComputeBandValue at U=0 and U=1)");
		Col.DefaultWidth = 90.0f;
		Col.ForRowKind = EArcModifierRowKind::Modifier;
		Col.CellFactory = &MakeModifierBandCell;
		Schema.Add(MoveTemp(Col));
	}

	return Schema;
}

} // namespace ArcRandomPool_ColumnSchema

#undef LOCTEXT_NAMESPACE
