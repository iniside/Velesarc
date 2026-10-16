// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSQueryExecution.h"
#include "ArcTQSQueryResult.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class SHeaderRow;

DECLARE_DELEGATE_OneParam(FArcTQSOnResultItemSelected, int32);

class SArcTQSResultsView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcTQSResultsView) {}
		SLATE_EVENT(FArcTQSOnResultItemSelected, OnItemSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetResult(const FArcTQSQueryResult& InResult, EArcTQSCreateFailure InFailure);

private:
	TSharedRef<ITableRow> GenerateRow(TSharedPtr<int32> ItemIndex, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleSelectionChanged(TSharedPtr<int32> ItemIndex, ESelectInfo::Type SelectInfo);
	void RebuildColumns();

	FText GetSummaryText() const;
	FText GetLevelOnlyText() const;
	EVisibility GetGeneratorBannerVisibility() const;
	EVisibility GetLevelOnlyVisibility() const;

	TSharedRef<FArcTQSQueryResult> Result = MakeShared<FArcTQSQueryResult>();
	EArcTQSCreateFailure Failure = EArcTQSCreateFailure::None;
	TArray<TSharedPtr<int32>> Rows;
	TArray<FString> ColumnStepNames;
	int32 SelectedItem = INDEX_NONE;

	TSharedPtr<SHeaderRow> HeaderRow;
	TSharedPtr<SListView<TSharedPtr<int32>>> ListView;
	FArcTQSOnResultItemSelected OnItemSelected;
};
