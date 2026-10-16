// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Input/Reply.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
class UArcEcotopeSpeciesModel;
struct FArcEcotopeBandSelection;

class SArcEcotopeBandList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcEcotopeBandList) {}
		SLATE_ARGUMENT_DEFAULT(UArcEcotopeSpeciesModel*, Species) { nullptr };
		SLATE_ARGUMENT(TSharedPtr<FArcEcotopeBandSelection>, Selection)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool RefreshAfterUndoRedo();

private:
	void Rebuild();
	void SyncListSelection();

	void HandleSelectionChanged();

	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<int32> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleListSelectionChanged(TSharedPtr<int32> Item, ESelectInfo::Type SelectInfo);

	FText GetBandLabel(int32 BandIndex) const;

	bool HasSelectedBand() const;

	FReply HandleAddBand();
	FReply HandleDuplicateBand();
	FReply HandleRemoveBand();

	TWeakObjectPtr<UArcEcotopeSpeciesModel> Species;
	TSharedPtr<FArcEcotopeBandSelection> Selection;
	TSharedPtr<SListView<TSharedPtr<int32>>> ListView;
	TArray<TSharedPtr<int32>> Items;
	bool bSyncingListSelection = false;
};
