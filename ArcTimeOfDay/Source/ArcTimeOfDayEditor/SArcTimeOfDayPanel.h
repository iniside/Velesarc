#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class AArcTimeOfDayActor;
class IDetailsView;
class UActorComponent;
struct FPropertyAndParent;

struct FArcTimeOfDayComponentItem
{
	TWeakObjectPtr<UActorComponent> Component;
	FText Label;
	FString SortKey;
};

class SArcTimeOfDayPanel final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcTimeOfDayPanel)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcTimeOfDayPanel() override;

private:
	enum class EPanelSelectionMode : uint8
	{
		None,
		Category,
		Component
	};

	void HandleEditorSelectionChanged(UObject* NewSelection);
	void RefreshFromEditorSelection();
	AArcTimeOfDayActor* ResolveSelectedActor() const;
	void RebuildCategoryItems();
	void RebuildComponentItems();
	bool IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const;
	bool IsCustomRowVisible(FName InRowName, FName InParentName) const;

	TSharedRef<ITableRow> GenerateCategoryRow(
		TSharedPtr<FName> Item,
		const TSharedRef<STableViewBase>& OwnerTable) const;
	TSharedRef<ITableRow> GenerateComponentRow(
		TSharedPtr<FArcTimeOfDayComponentItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable) const;
	void HandleCategorySelectionChanged(TSharedPtr<FName> Item, ESelectInfo::Type SelectInfo);
	void HandleComponentSelectionChanged(
		TSharedPtr<FArcTimeOfDayComponentItem> Item,
		ESelectInfo::Type SelectInfo);
	EVisibility GetEmptyStateVisibility() const;

	TSharedPtr<IDetailsView> DetailsView;
	TSharedPtr<SListView<TSharedPtr<FName>>> CategoryListView;
	TSharedPtr<SListView<TSharedPtr<FArcTimeOfDayComponentItem>>> ComponentListView;
	TArray<TSharedPtr<FName>> CategoryItems;
	TArray<TSharedPtr<FArcTimeOfDayComponentItem>> ComponentItems;
	TWeakObjectPtr<AArcTimeOfDayActor> TargetActor;
	TWeakObjectPtr<UActorComponent> SelectedComponent;
	FName SelectedCategory;
	FDelegateHandle SelectionChangedHandle;
	EPanelSelectionMode PanelSelectionMode = EPanelSelectionMode::None;
	bool bUpdatingListSelection = false;
};
