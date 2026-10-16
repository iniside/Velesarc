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

#include "Widgets/ArcModifierSharedCells.h"

#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "Widgets/SArcModifierTreeView.h"
#include "Shared/ArcCraftRecordToolkit.h"

#include "SGameplayTagContainerCombo.h"
#include "StructUtils/InstancedStruct.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/SNullWidget.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ArcModifierSharedCells"

namespace ArcModifierSharedCellsImpl
{

using FChipFormat = TFunction<FText(const FArcModifierRowEvaluation&)>;

/** Read-only chip over one row of the tree view's evaluation; green when the row (or its modifier) passed. */
static TSharedRef<SWidget> MakeEvaluationChip(
	TWeakPtr<SArcModifierTreeView> TreeViewWeak,
	int32 RowKey,
	bool bColorByModifier,
	FChipFormat Format)
{
	return SNew(STextBlock)
		.Text_Lambda([TreeViewWeak, RowKey, Format]() -> FText
		{
			const TSharedPtr<SArcModifierTreeView> TreeView = TreeViewWeak.Pin();
			const FArcModifierRowEvaluation* RowEval = TreeView.IsValid()
				? TreeView->GetCurrentEvaluation().ByRowKey.Find(RowKey)
				: nullptr;
			return RowEval != nullptr ? Format(*RowEval) : LOCTEXT("ChipNA", "—");
		})
		.Font(FAppStyle::GetFontStyle("SmallFont"))
		.ColorAndOpacity_Lambda([TreeViewWeak, RowKey, bColorByModifier]() -> FSlateColor
		{
			const TSharedPtr<SArcModifierTreeView> TreeView = TreeViewWeak.Pin();
			const FArcModifierRowEvaluation* RowEval = TreeView.IsValid()
				? TreeView->GetCurrentEvaluation().ByRowKey.Find(RowKey)
				: nullptr;
			if (RowEval == nullptr)
			{
				return FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f));
			}
			const bool bPassed = bColorByModifier ? RowEval->bModifierEligible : RowEval->bRulePassed;
			return bPassed
				? FSlateColor(FLinearColor(0.4f, 1.0f, 0.4f, 1.0f))
				: FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f));
		});
}

static const FNumberFormattingOptions& TwoDigits()
{
	static const FNumberFormattingOptions Options = FNumberFormattingOptions().SetMaximumFractionalDigits(2);
	return Options;
}

} // namespace ArcModifierSharedCellsImpl

TSharedRef<SWidget> ArcModifierSharedCells::MakeStatIdCell(UArcJsonRecordEditObject* Record, FModifierLocator Locate)
{
	const FInstancedStruct* ModStruct = Locate(Record);
	if (ModStruct == nullptr || ModStruct->GetPtr<FArcCraftModifier_Stats>() == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	return SNew(SInlineEditableTextBlock)
		.Text_Lambda([RecordWeak, Locate]() -> FText
		{
			const FInstancedStruct* Struct = Locate(RecordWeak.Get());
			const FArcCraftModifier_Stats* StatMod = Struct != nullptr ? Struct->GetPtr<FArcCraftModifier_Stats>() : nullptr;
			return StatMod != nullptr ? FText::FromName(StatMod->StatId) : FText::GetEmpty();
		})
		.OnTextCommitted_Lambda([RecordWeak, Locate](const FText& NewText, ETextCommit::Type)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FInstancedStruct* Struct = Locate(EditRecord);
			FArcCraftModifier_Stats* StatMod = Struct != nullptr ? Struct->GetMutablePtr<FArcCraftModifier_Stats>() : nullptr;
			if (StatMod == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetStatId", "Set Modifier Stat Id"), [StatMod, &NewText]()
			{
				StatMod->StatId = FName(*NewText.ToString());
			});
		})
		.Font(FAppStyle::GetFontStyle("SmallFont"));
}

TSharedRef<SWidget> ArcModifierSharedCells::MakeSlotTagsCell(UArcJsonRecordEditObject* Record, FModifierLocator Locate)
{
	const FInstancedStruct* ModStruct = Locate(Record);
	if (ModStruct == nullptr || ModStruct->GetPtr<FArcCraftModifier>() == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	const TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak(Record);
	return SNew(SGameplayTagContainerCombo)
		.Filter(TEXT("Modifier"))
		.TagContainer_Lambda([RecordWeak, Locate]() -> FGameplayTagContainer
		{
			const FInstancedStruct* Struct = Locate(RecordWeak.Get());
			const FArcCraftModifier* Mod = Struct != nullptr ? Struct->GetPtr<FArcCraftModifier>() : nullptr;
			return Mod != nullptr ? Mod->SlotTags : FGameplayTagContainer();
		})
		.OnTagContainerChanged_Lambda([RecordWeak, Locate](const FGameplayTagContainer& NewTags)
		{
			UArcJsonRecordEditObject* EditRecord = RecordWeak.Get();
			FInstancedStruct* Struct = Locate(EditRecord);
			FArcCraftModifier* Mod = Struct != nullptr ? Struct->GetMutablePtr<FArcCraftModifier>() : nullptr;
			if (Mod == nullptr) { return; }
			ArcCraftRecordToolkit::Edit(EditRecord, LOCTEXT("SetSlotTags", "Set Modifier Slot Tags"), [Mod, &NewTags]()
			{
				Mod->SlotTags = NewTags;
			});
		});
}

TSharedRef<SWidget> ArcModifierSharedCells::MakeBandCell(TWeakPtr<SArcModifierTreeView> TreeViewWeak, int32 RowKey)
{
	return ArcModifierSharedCellsImpl::MakeEvaluationChip(TreeViewWeak, RowKey, /*bColorByModifier=*/true,
		[](const FArcModifierRowEvaluation& RowEval) -> FText
		{
			if (!RowEval.bHasBand)
			{
				return LOCTEXT("BandNA", "—");
			}
			return FText::Format(LOCTEXT("BandRange", "{0} .. {1}"),
				FText::AsNumber(RowEval.BandAtU0, &ArcModifierSharedCellsImpl::TwoDigits()),
				FText::AsNumber(RowEval.BandAtU1, &ArcModifierSharedCellsImpl::TwoDigits()));
		});
}

TSharedRef<SWidget> ArcModifierSharedCells::MakeWeightCell(TWeakPtr<SArcModifierTreeView> TreeViewWeak, int32 RowKey)
{
	return ArcModifierSharedCellsImpl::MakeEvaluationChip(TreeViewWeak, RowKey, /*bColorByModifier=*/false,
		[](const FArcModifierRowEvaluation& RowEval) -> FText
		{
			return FText::AsNumber(RowEval.EffectiveWeight, &ArcModifierSharedCellsImpl::TwoDigits());
		});
}

TSharedRef<SWidget> ArcModifierSharedCells::MakeMatchedTagsCell(TWeakPtr<SArcModifierTreeView> TreeViewWeak, int32 RowKey)
{
	return ArcModifierSharedCellsImpl::MakeEvaluationChip(TreeViewWeak, RowKey, /*bColorByModifier=*/false,
		[](const FArcModifierRowEvaluation& RowEval) -> FText
		{
			return RowEval.MatchedTags.IsEmpty()
				? LOCTEXT("MatchedNone", "—")
				: FText::FromString(RowEval.MatchedTags.ToStringSimple());
		});
}

TSharedRef<SWidget> ArcModifierSharedCells::MakeIngredientShareCell(TWeakPtr<SArcModifierTreeView> TreeViewWeak, int32 RowKey)
{
	return ArcModifierSharedCellsImpl::MakeEvaluationChip(TreeViewWeak, RowKey, /*bColorByModifier=*/false,
		[](const FArcModifierRowEvaluation& RowEval) -> FText
		{
			return FText::AsPercent(RowEval.IngredientShare);
		});
}

#undef LOCTEXT_NAMESPACE
