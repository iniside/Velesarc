// Copyright Lukasz Baran. All Rights Reserved.

#include "SpeciesEditor/SArcEcotopeBandList.h"

#include "ScopedTransaction.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "Species/ArcForestStageBand.h"
#include "SpeciesEditor/ArcEcotopeSpeciesEditor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SArcEcotopeBandList"

namespace ArcEcotopeBandListPrivate
{
	FText MakeVisualTag(const FArcForestStageBand& Band)
	{
		const UScriptStruct* VisualType = Band.Visual.GetScriptStruct();
		if (VisualType == FArcForestBandVisual_StaticMesh::StaticStruct())
		{
			return LOCTEXT("VisualStatic", "Static");
		}
		if (VisualType == FArcForestBandVisual_SkinnedMesh::StaticStruct())
		{
			return LOCTEXT("VisualSkinned", "Skinned");
		}
		if (VisualType == FArcForestBandVisual_Entity::StaticStruct())
		{
			return LOCTEXT("VisualEntity", "Entity");
		}
		return FText::FromString(TEXT("\u2014"));
	}

	FText MakeHeightText(float HeightCm)
	{
		FNumberFormattingOptions Options;
		Options.SetUseGrouping(false);
		Options.SetMinimumFractionalDigits(0);
		Options.SetMaximumFractionalDigits(2);
		return FText::AsNumber(HeightCm, &Options);
	}
}

void SArcEcotopeBandList::Construct(const FArguments& InArgs)
{
	Species = InArgs._Species;
	Selection = InArgs._Selection;

	check(Selection.IsValid());
	Selection->OnChanged.AddSP(this, &SArcEcotopeBandList::HandleSelectionChanged);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddBandButton", "Add"))
				.ToolTipText(LOCTEXT("AddBandTooltip", "Add a new stage band."))
				.OnClicked(this, &SArcEcotopeBandList::HandleAddBand)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("DuplicateBandButton", "Duplicate"))
				.ToolTipText(LOCTEXT("DuplicateBandTooltip", "Duplicate the selected stage band."))
				.IsEnabled(TAttribute<bool>::CreateSP(this, &SArcEcotopeBandList::HasSelectedBand))
				.OnClicked(this, &SArcEcotopeBandList::HandleDuplicateBand)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveBandButton", "Remove"))
				.ToolTipText(LOCTEXT("RemoveBandTooltip", "Remove the selected stage band."))
				.IsEnabled(TAttribute<bool>::CreateSP(this, &SArcEcotopeBandList::HasSelectedBand))
				.OnClicked(this, &SArcEcotopeBandList::HandleRemoveBand)
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<int32>>)
			.ListItemsSource(&Items)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SArcEcotopeBandList::HandleGenerateRow)
			.OnSelectionChanged(this, &SArcEcotopeBandList::HandleListSelectionChanged)
		]
	];

	Rebuild();
	SyncListSelection();
}

bool SArcEcotopeBandList::RefreshAfterUndoRedo()
{
	Rebuild();

	const int32 CurrentIndex = Selection->GetBandIndex();
	const int32 ClampedIndex = CurrentIndex < Items.Num() ? CurrentIndex : Items.Num() - 1;

	if (ClampedIndex != CurrentIndex)
	{
		Selection->SetBandIndex(ClampedIndex);
		return true;
	}

	SyncListSelection();
	return false;
}

void SArcEcotopeBandList::HandleSelectionChanged()
{
	SyncListSelection();
}

void SArcEcotopeBandList::Rebuild()
{
	Items.Reset();

	UArcEcotopeSpeciesModel* Model = Species.Get();
	if (Model != nullptr)
	{
		Items.Reserve(Model->Bands.Num());
		for (int32 BandIndex = 0; BandIndex < Model->Bands.Num(); ++BandIndex)
		{
			Items.Add(MakeShared<int32>(BandIndex));
		}
	}

	if (ListView.IsValid())
	{
		TGuardValue<bool> SyncGuard(bSyncingListSelection, true);
		ListView->ClearSelection();
		ListView->RequestListRefresh();
	}
}

void SArcEcotopeBandList::SyncListSelection()
{
	if (!ListView.IsValid())
	{
		return;
	}

	const int32 BandIndex = Selection->GetBandIndex();

	TGuardValue<bool> SyncGuard(bSyncingListSelection, true);
	if (Items.IsValidIndex(BandIndex))
	{
		ListView->SetSelection(Items[BandIndex], ESelectInfo::Direct);
		ListView->RequestScrollIntoView(Items[BandIndex]);
	}
	else
	{
		ListView->ClearSelection();
	}
}

TSharedRef<ITableRow> SArcEcotopeBandList::HandleGenerateRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const int32 BandIndex = Item.IsValid() ? *Item : INDEX_NONE;

	return SNew(STableRow<TSharedPtr<int32>>, OwnerTable)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(STextBlock)
			.Text(TAttribute<FText>::CreateSP(this, &SArcEcotopeBandList::GetBandLabel, BandIndex))
		];
}

void SArcEcotopeBandList::HandleListSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type SelectInfo)
{
	if (bSyncingListSelection)
	{
		return;
	}

	const int32 BandIndex = Item.IsValid() ? *Item : INDEX_NONE;
	if (BandIndex != Selection->GetBandIndex())
	{
		Selection->SetBandIndex(BandIndex);
	}
}

FText SArcEcotopeBandList::GetBandLabel(int32 BandIndex) const
{
	const UArcEcotopeSpeciesModel* Model = Species.Get();
	if (Model == nullptr || !Model->Bands.IsValidIndex(BandIndex))
	{
		return FText::GetEmpty();
	}

	const FArcForestStageBand& Band = Model->Bands[BandIndex];
	const FText KindText = StaticEnum<EArcForestVisualKind>()->GetDisplayNameTextByValue(static_cast<int64>(Band.Kind));

	return FText::Format(
		LOCTEXT("BandLabel", "{0} {1}-{2} cm  [{3}]"),
		KindText,
		ArcEcotopeBandListPrivate::MakeHeightText(Band.MinHeightCm),
		ArcEcotopeBandListPrivate::MakeHeightText(Band.MaxHeightCm),
		ArcEcotopeBandListPrivate::MakeVisualTag(Band));
}

bool SArcEcotopeBandList::HasSelectedBand() const
{
	const UArcEcotopeSpeciesModel* Model = Species.Get();
	return Model != nullptr && Model->Bands.IsValidIndex(Selection->GetBandIndex());
}

FReply SArcEcotopeBandList::HandleAddBand()
{
	UArcEcotopeSpeciesModel* Model = Species.Get();
	if (Model == nullptr)
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("AddBand", "Add Stage Band"));
	Model->Modify();
	const int32 NewIndex = Model->Bands.AddDefaulted();
	Model->PostEditChange();
	Rebuild();
	Selection->SetBandIndex(NewIndex);

	return FReply::Handled();
}

FReply SArcEcotopeBandList::HandleDuplicateBand()
{
	UArcEcotopeSpeciesModel* Model = Species.Get();
	const int32 SourceIndex = Selection->GetBandIndex();
	if (Model == nullptr || !Model->Bands.IsValidIndex(SourceIndex))
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("DuplicateBand", "Duplicate Stage Band"));
	Model->Modify();
	const FArcForestStageBand SourceBand = Model->Bands[SourceIndex];
	const int32 NewIndex = Model->Bands.Insert(SourceBand, SourceIndex + 1);
	Model->PostEditChange();
	Rebuild();
	Selection->SetBandIndex(NewIndex);

	return FReply::Handled();
}

FReply SArcEcotopeBandList::HandleRemoveBand()
{
	UArcEcotopeSpeciesModel* Model = Species.Get();
	const int32 RemovedIndex = Selection->GetBandIndex();
	if (Model == nullptr || !Model->Bands.IsValidIndex(RemovedIndex))
	{
		return FReply::Handled();
	}

	// The broadcast has to tear down the details panel's live property nodes before the element dies.
	Selection->SetBandIndex(INDEX_NONE);

	FScopedTransaction Transaction(LOCTEXT("RemoveBand", "Remove Stage Band"));
	Model->Modify();
	Model->Bands.RemoveAt(RemovedIndex);
	Model->PostEditChange();
	Rebuild();
	Selection->SetBandIndex(Model->Bands.IsValidIndex(RemovedIndex) ? RemovedIndex : Model->Bands.Num() - 1);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
