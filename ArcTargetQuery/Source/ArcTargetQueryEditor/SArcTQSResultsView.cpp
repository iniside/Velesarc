// Copyright Lukasz Baran. All Rights Reserved.

#include "SArcTQSResultsView.h"

#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SArcTQSResultsView"

namespace ArcTQSResultsViewPrivate
{
	const FName IndexColumn(TEXT("Index"));
	const FName LocationColumn(TEXT("Location"));
	const FName ScoreColumn(TEXT("Score"));
	const FName ValidColumn(TEXT("Valid"));
	const FName WinnerColumn(TEXT("Winner"));
	const FName FilteredByColumn(TEXT("FilteredBy"));
	const FName StepColumnBase(TEXT("Step"));

	FName MakeStepColumn(const int32 StepIndex)
	{
		return FName(StepColumnBase, StepIndex + 1);
	}

	int32 GetStepIndex(const FName ColumnName)
	{
		if (ColumnName.GetComparisonIndex() != StepColumnBase.GetComparisonIndex() || ColumnName.GetNumber() == 0)
		{
			return INDEX_NONE;
		}
		return ColumnName.GetNumber() - 1;
	}

	FString FormatStepCell(const FArcTQSStepResult& Step, const int32 ItemIndex)
	{
		switch (Step.State)
		{
		case EArcTQSStepState::NotRun:
			return TEXT("NotRun");
		case EArcTQSStepState::Skipped:
			return TEXT("Skipped");
		case EArcTQSStepState::Null:
			return TEXT("Null");
		case EArcTQSStepState::Executed:
			break;
		}
		if (!Step.RawScores.IsValidIndex(ItemIndex))
		{
			return TEXT("-");
		}
		const float Cumulative = Step.CumulativeScores.IsValidIndex(ItemIndex) ? Step.CumulativeScores[ItemIndex] : 0.0f;
		return FString::Printf(TEXT("%.3f / %.3f"), Step.RawScores[ItemIndex], Cumulative);
	}

	class SResultRow : public SMultiColumnTableRow<TSharedPtr<int32>>
	{
	public:
		SLATE_BEGIN_ARGS(SResultRow) {}
		SLATE_END_ARGS()

		void Construct(
			const FArguments& InArgs,
			const TSharedRef<STableViewBase>& InOwnerTable,
			const TSharedRef<const FArcTQSQueryResult>& InResult,
			const int32 InItemIndex)
		{
			Result = InResult;
			ItemIndex = InItemIndex;
			SMultiColumnTableRow<TSharedPtr<int32>>::Construct(FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			return SNew(STextBlock)
				.Margin(FMargin(4.0f, 1.0f))
				.Text(FText::FromString(GetCellString(ColumnName)));
		}

	private:
		FString GetCellString(const FName ColumnName) const
		{
			const FArcTQSTargetItem& Item = Result->Items[ItemIndex];
			if (ColumnName == IndexColumn)
			{
				return FString::FromInt(ItemIndex);
			}
			if (ColumnName == LocationColumn)
			{
				return FString::Printf(TEXT("%.0f, %.0f, %.0f"), Item.Location.X, Item.Location.Y, Item.Location.Z);
			}
			if (ColumnName == ScoreColumn)
			{
				return FString::Printf(TEXT("%.3f"), Item.Score);
			}
			if (ColumnName == ValidColumn)
			{
				return Item.bValid ? TEXT("Yes") : TEXT("No");
			}
			if (ColumnName == WinnerColumn)
			{
				return Result->ResultIndices.Contains(ItemIndex) ? TEXT("Yes") : FString();
			}
			if (ColumnName == FilteredByColumn)
			{
				const int32 StepIndex = Result->FilteredByStep.IsValidIndex(ItemIndex) ? Result->FilteredByStep[ItemIndex] : INDEX_NONE;
				return Result->Steps.IsValidIndex(StepIndex) ? Result->Steps[StepIndex].StepName : FString();
			}
			const int32 StepIndex = GetStepIndex(ColumnName);
			if (Result->Steps.IsValidIndex(StepIndex))
			{
				return FormatStepCell(Result->Steps[StepIndex], ItemIndex);
			}
			return FString();
		}

		TSharedPtr<const FArcTQSQueryResult> Result;
		int32 ItemIndex = INDEX_NONE;
	};
}

void SArcTQSResultsView::Construct(const FArguments& InArgs)
{
	OnItemSelected = InArgs._OnItemSelected;

	HeaderRow = SNew(SHeaderRow);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcTQSResultsView::GetSummaryText)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(SBorder)
			.Visibility(this, &SArcTQSResultsView::GetGeneratorBannerVisibility)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(4.0f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.1f))
				.Text(LOCTEXT("GeneratorLevelOnly", "The generator needs world services that the asset preview lacks. It is level only and produced no items here."))
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(STextBlock)
			.Visibility(this, &SArcTQSResultsView::GetLevelOnlyVisibility)
			.ColorAndOpacity(FLinearColor(1.0f, 0.6f, 0.1f))
			.Text(this, &SArcTQSResultsView::GetLevelOnlyText)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(ListView, SListView<TSharedPtr<int32>>)
			.ListItemsSource(&Rows)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SArcTQSResultsView::GenerateRow)
			.OnSelectionChanged(this, &SArcTQSResultsView::HandleSelectionChanged)
			.HeaderRow(HeaderRow)
		]
	];

	RebuildColumns();
}

void SArcTQSResultsView::SetResult(const FArcTQSQueryResult& InResult, const EArcTQSCreateFailure InFailure)
{
	Result = MakeShared<FArcTQSQueryResult>(InResult);
	Failure = InFailure;

	TArray<FString> StepNames;
	StepNames.Reserve(Result->Steps.Num());
	for (const FArcTQSStepResult& Step : Result->Steps)
	{
		StepNames.Add(Step.StepName);
	}
	if (StepNames != ColumnStepNames)
	{
		ColumnStepNames = MoveTemp(StepNames);
		RebuildColumns();
	}

	Rows.Reset(Result->Items.Num());
	TSharedPtr<int32> SelectedRow;
	for (int32 ItemIndex = 0; ItemIndex < Result->Items.Num(); ++ItemIndex)
	{
		Rows.Add(MakeShared<int32>(ItemIndex));
		if (ItemIndex == SelectedItem)
		{
			SelectedRow = Rows.Last();
		}
	}

	if (ListView.IsValid())
	{
		ListView->RebuildList();
		if (SelectedRow.IsValid())
		{
			ListView->SetSelection(SelectedRow, ESelectInfo::Direct);
		}
	}
}

TSharedRef<ITableRow> SArcTQSResultsView::GenerateRow(TSharedPtr<int32> ItemIndex, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(ArcTQSResultsViewPrivate::SResultRow, OwnerTable, Result, *ItemIndex);
}

void SArcTQSResultsView::HandleSelectionChanged(TSharedPtr<int32> ItemIndex, ESelectInfo::Type SelectInfo)
{
	SelectedItem = ItemIndex.IsValid() ? *ItemIndex : INDEX_NONE;
	OnItemSelected.ExecuteIfBound(SelectedItem);
}

void SArcTQSResultsView::RebuildColumns()
{
	using namespace ArcTQSResultsViewPrivate;

	HeaderRow->ClearColumns();
	HeaderRow->AddColumn(SHeaderRow::Column(IndexColumn)
		.DefaultLabel(LOCTEXT("IndexColumn", "#"))
		.ManualWidth(40.0f));
	HeaderRow->AddColumn(SHeaderRow::Column(LocationColumn)
		.DefaultLabel(LOCTEXT("LocationColumn", "Location"))
		.ManualWidth(150.0f));
	HeaderRow->AddColumn(SHeaderRow::Column(ScoreColumn)
		.DefaultLabel(LOCTEXT("ScoreColumn", "Score"))
		.ManualWidth(60.0f));
	HeaderRow->AddColumn(SHeaderRow::Column(ValidColumn)
		.DefaultLabel(LOCTEXT("ValidColumn", "Valid"))
		.ManualWidth(50.0f));
	HeaderRow->AddColumn(SHeaderRow::Column(WinnerColumn)
		.DefaultLabel(LOCTEXT("WinnerColumn", "Winner"))
		.ManualWidth(55.0f));
	HeaderRow->AddColumn(SHeaderRow::Column(FilteredByColumn)
		.DefaultLabel(LOCTEXT("FilteredByColumn", "Filtered By"))
		.ManualWidth(120.0f));
	for (int32 StepIndex = 0; StepIndex < ColumnStepNames.Num(); ++StepIndex)
	{
		HeaderRow->AddColumn(SHeaderRow::Column(MakeStepColumn(StepIndex))
			.DefaultLabel(FText::Format(LOCTEXT("StepColumn", "{0}. {1}"), FText::AsNumber(StepIndex), FText::FromString(ColumnStepNames[StepIndex])))
			.DefaultTooltip(LOCTEXT("StepColumnTooltip", "Raw / cumulative score of this step"))
			.ManualWidth(130.0f));
	}
}

FText SArcTQSResultsView::GetSummaryText() const
{
	switch (Failure)
	{
	case EArcTQSCreateFailure::NoDefinition:
		return LOCTEXT("FailureNoDefinition", "Not run: the preview reference has no definition.");
	case EArcTQSCreateFailure::NotExecutable:
		return LOCTEXT("FailureNotExecutable", "Not run: the definition does not compile.");
	case EArcTQSCreateFailure::LayoutMismatch:
		return LOCTEXT("FailureLayoutMismatch", "Not run: the preview parameters do not match the definition.");
	case EArcTQSCreateFailure::None:
		break;
	}

	return FText::Format(
		LOCTEXT("Summary", "Status: {0}   Generated: {1}   Valid: {2}   Results: {3}   Time: {4} ms"),
		StaticEnum<EArcTQSQueryStatus>()->GetDisplayNameTextByValue(static_cast<int64>(Result->Status)),
		FText::AsNumber(Result->TotalGenerated),
		FText::AsNumber(Result->TotalValid),
		FText::AsNumber(Result->ResultIndices.Num()),
		FText::FromString(FString::Printf(TEXT("%.3f"), Result->ExecutionTimeMs)));
}

FText SArcTQSResultsView::GetLevelOnlyText() const
{
	TArray<FString> StepNames;
	for (const FArcTQSStepResult& Step : Result->Steps)
	{
		if (Step.State == EArcTQSStepState::Skipped)
		{
			StepNames.Add(Step.StepName);
		}
	}
	return FText::Format(
		LOCTEXT("LevelOnlySteps", "Level only, skipped here: {0}"),
		FText::FromString(FString::Join(StepNames, TEXT(", "))));
}

EVisibility SArcTQSResultsView::GetGeneratorBannerVisibility() const
{
	return Result->bGeneratorLevelOnly ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SArcTQSResultsView::GetLevelOnlyVisibility() const
{
	for (const FArcTQSStepResult& Step : Result->Steps)
	{
		if (Step.State == EArcTQSStepState::Skipped)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
