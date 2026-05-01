// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STreeView.h"
#include "MassProcessorBrowser/ArcMassProcessorBrowserModel.h"

class IDetailsView;
class UMassProcessor;
class UArcMassProcessorBrowserConfig;
class SSearchBox;

class SArcMassProcessorBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcMassProcessorBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcProcessorTreeItem> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(TSharedPtr<FArcProcessorTreeItem> Item, TArray<TSharedPtr<FArcProcessorTreeItem>>& OutChildren);
	void OnSelectionChanged(TSharedPtr<FArcProcessorTreeItem> Item, ESelectInfo::Type SelectInfo);

	void OnSearchTextChanged(const FText& InText);
	void RebuildFilteredTree();
	bool DoesItemMatchFilter(const TSharedPtr<FArcProcessorTreeItem>& Item) const;

	void RefreshInfoPanel(UMassProcessor* ProcessorCDO);
	void OnRefreshClicked();
	void NavigateToProcessor(const UClass* ProcessorClass);

	FArcMassProcessorBrowserModel Model;
	TWeakObjectPtr<UArcMassProcessorBrowserConfig> ConfigAsset;

	TSharedPtr<STreeView<TSharedPtr<FArcProcessorTreeItem>>> TreeView;
	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<SVerticalBox> InfoPanelBox;

	FString FilterText;
	TArray<TSharedPtr<FArcProcessorTreeItem>> FilteredRootItems;
	TWeakObjectPtr<UMassProcessor> SelectedProcessor;
};
