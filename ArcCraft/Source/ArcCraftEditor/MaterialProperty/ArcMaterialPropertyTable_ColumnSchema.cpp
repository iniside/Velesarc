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

#include "MaterialProperty/ArcMaterialPropertyTable_ColumnSchema.h"

#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableData.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "Widgets/ArcModifierTreeItem.h"
#include "Widgets/ArcModifierColumnSchema.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "Widgets/ArcModifierSharedCells.h"
#include "Widgets/SArcModifierTreeView.h"
#include "Shared/ArcCraftRecordToolkit.h"

#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"
#include "GameplayTagContainer.h"
#include "SGameplayTagQueryEntryBox.h"

#define LOCTEXT_NAMESPACE "ArcMaterialPropertyTable_ColumnSchema"

namespace ArcMaterialPropertyTable_ColumnSchema_Impl
{

static FArcMaterialPropertyRule* FindRule(UArcJsonRecordEditObject* Record, int32 RuleIdx)
{
	FArcMaterialPropertyTableData* Table = ArcCraftRecordToolkit::GetData<FArcMaterialPropertyTableData>(Record);
	if (Table == nullptr || !Table->Rules.IsValidIndex(RuleIdx))
	{
		return nullptr;
	}
	return &Table->Rules[RuleIdx];
}

static FInstancedStruct* FindModifierStruct(UArcJsonRecordEditObject* Record, int32 RuleIdx, int32 ModIdx)
{
	FArcMaterialPropertyRule* Rule = FindRule(Record, RuleIdx);
	if (Rule == nullptr || !Rule->Modifiers.IsValidIndex(ModIdx))
	{
		return nullptr;
	}
	return &Rule->Modifiers[ModIdx];
}

static FArcCraftModifier* FindModifier(UArcJsonRecordEditObject* Record, int32 RuleIdx, int32 ModIdx)
{
	FInstancedStruct* ModStruct = FindModifierStruct(Record, RuleIdx, ModIdx);
	return ModStruct != nullptr ? ModStruct->GetMutablePtr<FArcCraftModifier>() : nullptr;
}

// ---------------------------------------------------------------------------
// Rule-row cell factories
// ---------------------------------------------------------------------------

static TSharedRef<SWidget> MakeRuleNameCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindRule(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 RuleIdx = Item.ChildIndex;
	return SNew(SInlineEditableTextBlock)
		.Text_Lambda([RecordWeak, RuleIdx]() -> FText
		{
			const FArcMaterialPropertyRule* Rule = FindRule(RecordWeak.Get(), RuleIdx);
			return Rule != nullptr ? Rule->RuleName : FText::GetEmpty();
		})
		.OnTextCommitted_Lambda([RecordWeak, RuleIdx](const FText& NewText, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcMaterialPropertyRule* Rule = FindRule(EditRecord, RuleIdx);
			if (Rule == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetRuleName", "Set Rule Name"), [Rule, &NewText]()
			{
				Rule->RuleName = NewText;
			});
		})
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeRuleTagQueryCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindRule(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 RuleIdx = Item.ChildIndex;
	return SNew(SGameplayTagQueryEntryBox)
		.TagQuery_Lambda([RecordWeak, RuleIdx]() -> FGameplayTagQuery
		{
			const FArcMaterialPropertyRule* Rule = FindRule(RecordWeak.Get(), RuleIdx);
			return Rule != nullptr ? Rule->TagQuery : FGameplayTagQuery();
		})
		.OnTagQueryChanged_Lambda([RecordWeak, RuleIdx](const FGameplayTagQuery& NewQuery)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcMaterialPropertyRule* Rule = FindRule(EditRecord, RuleIdx);
			if (Rule == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetTagQuery", "Set Rule Tag Query"), [Rule, &NewQuery]()
			{
				Rule->TagQuery = NewQuery;
			});
		});
}

static TSharedRef<SWidget> MakeRuleMinAverageRCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindRule(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 RuleIdx = Item.ChildIndex;
	return SNew(SNumericEntryBox<float>)
		.Value_Lambda([RecordWeak, RuleIdx]() -> TOptional<float>
		{
			const FArcMaterialPropertyRule* Rule = FindRule(RecordWeak.Get(), RuleIdx);
			return Rule != nullptr ? TOptional<float>(Rule->MinAverageR) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([RecordWeak, RuleIdx](float NewVal, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcMaterialPropertyRule* Rule = FindRule(EditRecord, RuleIdx);
			if (Rule == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetRuleMinQ", "Set Rule Min Average R"), [Rule, NewVal]()
			{
				Rule->MinAverageR = FMath::Max(0.0f, NewVal);
			});
		})
		.MinValue(0.0f)
		.MaxValue(2.0f)
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeRuleWeightCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindRule(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 RuleIdx = Item.ChildIndex;
	return SNew(SNumericEntryBox<float>)
		.Value_Lambda([RecordWeak, RuleIdx]() -> TOptional<float>
		{
			const FArcMaterialPropertyRule* Rule = FindRule(RecordWeak.Get(), RuleIdx);
			return Rule != nullptr ? TOptional<float>(Rule->Weight) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([RecordWeak, RuleIdx](float NewVal, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcMaterialPropertyRule* Rule = FindRule(EditRecord, RuleIdx);
			if (Rule == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetRuleWeight", "Set Rule Weight"), [Rule, NewVal]()
			{
				Rule->Weight = FMath::Max(0.0f, NewVal);
			});
		})
		.MinValue(0.0f)
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeRuleRWeightScalingCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindRule(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 RuleIdx = Item.ChildIndex;
	return SNew(SNumericEntryBox<float>)
		.Value_Lambda([RecordWeak, RuleIdx]() -> TOptional<float>
		{
			const FArcMaterialPropertyRule* Rule = FindRule(RecordWeak.Get(), RuleIdx);
			return Rule != nullptr ? TOptional<float>(Rule->RWeightScaling) : TOptional<float>();
		})
		.OnValueCommitted_Lambda([RecordWeak, RuleIdx](float NewVal, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FArcMaterialPropertyRule* Rule = FindRule(EditRecord, RuleIdx);
			if (Rule == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetRuleRWeightScaling", "Set Rule R Weight Scaling"), [Rule, NewVal]()
			{
				Rule->RWeightScaling = NewVal;
			});
		})
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeRuleWeightModifiersCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> /*TreeViewWeak*/)
{
	if (FindRule(Record, Item.ChildIndex) == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	const int32 RuleIdx = Item.ChildIndex;
	return SNew(STextBlock)
		.Text_Lambda([RecordWeak, RuleIdx]() -> FText
		{
			const FArcMaterialPropertyRule* Rule = FindRule(RecordWeak.Get(), RuleIdx);
			return Rule != nullptr ? FText::AsNumber(Rule->WeightModifiers.Num()) : FText::GetEmpty();
		})
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

static TSharedRef<SWidget> MakeRuleEffectiveWeightCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> TreeViewWeak)
{
	return FindRule(Record, Item.ChildIndex) != nullptr
		? ArcModifierSharedCells::MakeWeightCell(TreeViewWeak, Item.ChildIndex)
		: SNullWidget::NullWidget;
}

static TSharedRef<SWidget> MakeRuleMatchedTagsCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> TreeViewWeak)
{
	return FindRule(Record, Item.ChildIndex) != nullptr
		? ArcModifierSharedCells::MakeMatchedTagsCell(TreeViewWeak, Item.ChildIndex)
		: SNullWidget::NullWidget;
}

static TSharedRef<SWidget> MakeRuleIngredientShareCell(UArcJsonRecordEditObject* Record, FArcModifierTreeItem& Item, int32 RowKey,
	TWeakPtr<SArcModifierTreeView> TreeViewWeak)
{
	return FindRule(Record, Item.ChildIndex) != nullptr
		? ArcModifierSharedCells::MakeIngredientShareCell(TreeViewWeak, Item.ChildIndex)
		: SNullWidget::NullWidget;
}

// ---------------------------------------------------------------------------
// Modifier child-row cell factories
// ---------------------------------------------------------------------------

// Subtype is now changed via the row details panel (FInstancedStruct picker).
// This cell is a read-only label showing the current subtype's display name.
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

static ArcModifierSharedCells::FModifierLocator MakeModifierLocator(int32 RuleIdx, int32 ModIdx)
{
	return [RuleIdx, ModIdx](UArcJsonRecordEditObject* Record) -> FInstancedStruct*
	{
		return FindModifierStruct(Record, RuleIdx, ModIdx);
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

} // namespace ArcMaterialPropertyTable_ColumnSchema_Impl

// ---------------------------------------------------------------------------
// Public schema constructors
// ---------------------------------------------------------------------------

namespace ArcMaterialPropertyTable_ColumnSchema
{

TArray<FArcModifierColumn> GetRuleRowSchema()
{
	using namespace ArcMaterialPropertyTable_ColumnSchema_Impl;

	TArray<FArcModifierColumn> Schema;

	// RuleName
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleName");
		Col.Label = LOCTEXT("RuleNameCol", "Rule Name");
		Col.Tooltip = LOCTEXT("RuleNameColTip", "Editable name for this rule");
		Col.DefaultWidth = 140.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleNameCell;
		Schema.Add(MoveTemp(Col));
	}

	// TagQuery
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleTagQuery");
		Col.Label = LOCTEXT("RuleTagQueryCol", "Tag Query");
		Col.Tooltip = LOCTEXT("RuleTagQueryColTip", "Gameplay tag query filter — rule applies when ingredient tags match");
		Col.DefaultWidth = 200.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleTagQueryCell;
		Schema.Add(MoveTemp(Col));
	}

	// MinAverageR
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleMinQ");
		Col.Label = LOCTEXT("RuleMinQCol", "Min Average R");
		Col.Tooltip = LOCTEXT("RuleMinQColTip", "Minimum average ingredient R required for this rule to be eligible");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleMinAverageRCell;
		Schema.Add(MoveTemp(Col));
	}

	// RuleWeight
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleWeight");
		Col.Label = LOCTEXT("RuleWeightCol", "Weight");
		Col.Tooltip = LOCTEXT("RuleWeightColTip", "Base weight of this rule among the matched rules competing for chairs");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleWeightCell;
		Schema.Add(MoveTemp(Col));
	}

	// RuleRWeightScaling
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleRWeightScaling");
		Col.Label = LOCTEXT("RuleRWeightScalingCol", "R Scaling");
		Col.Tooltip = LOCTEXT("RuleRWeightScalingColTip", "Weight *= 1 + (average R - 1) * R Scaling; 0 = R has no effect");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleRWeightScalingCell;
		Schema.Add(MoveTemp(Col));
	}

	// RuleWeightModifiers
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleWeightModifiers");
		Col.Label = LOCTEXT("RuleWeightModifiersCol", "Weight Mods");
		Col.Tooltip = LOCTEXT("RuleWeightModifiersColTip", "Number of tag-conditional weight modifiers (edit them in the row details)");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleWeightModifiersCell;
		Schema.Add(MoveTemp(Col));
	}

	// RuleEffWeight
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleEffWeight");
		Col.Label = LOCTEXT("RuleEffWeightCol", "Eff. Weight");
		Col.Tooltip = LOCTEXT("RuleEffWeightColTip", "Debug: rule weight for the debug ingredients (FArcMaterialCraftEvaluator::ComputeRuleWeight)");
		Col.DefaultWidth = 60.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleEffectiveWeightCell;
		Schema.Add(MoveTemp(Col));
	}

	// RuleMatchedTags
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleMatchedTags");
		Col.Label = LOCTEXT("RuleMatchedTagsCol", "Matched Tags");
		Col.Tooltip = LOCTEXT("RuleMatchedTagsColTip", "Debug: positive tags of the rule query that the debug ingredients hold");
		Col.DefaultWidth = 160.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleMatchedTagsCell;
		Schema.Add(MoveTemp(Col));
	}

	// RuleIngredientShare
	{
		FArcModifierColumn Col;
		Col.Id = FName("RuleIngredientShare");
		Col.Label = LOCTEXT("RuleIngredientShareCol", "Share");
		Col.Tooltip = LOCTEXT("RuleIngredientShareColTip", "Debug: share of the consumed amount in the slots holding the rule query tags");
		Col.DefaultWidth = 50.0f;
		Col.ForRowKind = EArcModifierRowKind::Rule;
		Col.CellFactory = &MakeRuleIngredientShareCell;
		Schema.Add(MoveTemp(Col));
	}

	return Schema;
}

TArray<FArcModifierColumn> GetModifierChildRowSchema()
{
	using namespace ArcMaterialPropertyTable_ColumnSchema_Impl;

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

	// MinR (modifier threshold, checked against Ctx.AverageR directly)
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

} // namespace ArcMaterialPropertyTable_ColumnSchema

#undef LOCTEXT_NAMESPACE
