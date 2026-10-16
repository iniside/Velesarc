// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTacticsEditorClipboard.h"
#include "Misc/Guid.h"
#include "Widgets/SCompoundWidget.h"

class FArcTacticsEditorViewModel;
class FUICommandList;
class ITableRow;
class SSearchBox;
class STableViewBase;
class SArcTacticsNodeRow;
template <typename ItemType> class STreeView;
enum class EArcTacticsAuthoringChange : uint8;
namespace ESelectInfo { enum Type : int; }

enum class EArcTacticsViewGroup : uint8
{
	None,
	Context,
	GlobalTasks,
	Decisions,
	Actions,
	Selectors,
	ActiveSteps
};

struct FArcTacticsViewItem
{
	EArcTacticsViewGroup Group = EArcTacticsViewGroup::None;
	FArcTacticsEditorSelection Selection;
	FText Label;
	TArray<TSharedPtr<FArcTacticsViewItem>> Children;

	bool IsGroup() const { return Group != EArcTacticsViewGroup::None; }
};

struct FArcTacticsViewItemKey
{
	EArcTacticsViewGroup Group = EArcTacticsViewGroup::None;
	FArcTacticsEditorSelection Selection;

	friend bool operator==(const FArcTacticsViewItemKey&, const FArcTacticsViewItemKey&) = default;
	friend uint32 GetTypeHash(const FArcTacticsViewItemKey& Key)
	{
		return HashCombine(
			::GetTypeHash(static_cast<uint8>(Key.Group)),
			HashCombine(
				::GetTypeHash(static_cast<uint8>(Key.Selection.Kind)),
				GetTypeHash(Key.Selection.ID)));
	}
};

class SArcTacticsView final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcTacticsView) {}
	SLATE_END_ARGS()

	virtual ~SArcTacticsView() override;
	void Construct(
		const FArguments& InArgs,
		TSharedRef<FArcTacticsEditorViewModel> InViewModel,
		TSharedRef<FUICommandList> InCommandList);

private:
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }

	void BindCommands();
	void RebuildItems(bool bPreserveExpansion);
	void BuildItems(TArray<TSharedPtr<FArcTacticsViewItem>>& OutItems) const;
	TSharedPtr<FArcTacticsViewItem> FilterItem(
		const TSharedPtr<FArcTacticsViewItem>& Item,
		const FString& Filter) const;
	bool ItemMatchesFilter(const FArcTacticsViewItem& Item, const FString& Filter) const;
	FArcTacticsViewItemKey MakeKey(const FArcTacticsViewItem& Item) const;
	void CaptureExpansion(TSet<FArcTacticsViewItemKey>& OutKeys) const;
	void RestoreExpansion(const TSet<FArcTacticsViewItemKey>& Keys);
	void ExpandAll();
	void CollapseAll();

	TSharedRef<ITableRow> HandleGenerateRow(
		TSharedPtr<FArcTacticsViewItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);
	void HandleGetChildren(
		TSharedPtr<FArcTacticsViewItem> Item,
		TArray<TSharedPtr<FArcTacticsViewItem>>& OutChildren) const;
	void HandleSelectionChanged(
		TSharedPtr<FArcTacticsViewItem> Item,
		ESelectInfo::Type SelectInfo);
	void HandleExpansionChanged(TSharedPtr<FArcTacticsViewItem> Item, bool bExpanded);
	void HandleDoubleClick(TSharedPtr<FArcTacticsViewItem> Item);
	TSharedPtr<SWidget> HandleContextMenuOpening();
	void HandleSearchChanged(const FText& Text);
	void HandleModelChanged(EArcTacticsAuthoringChange Change);
	void HandleModelSelectionChanged(const FArcTacticsEditorSelection& Selection);
	void SynchronizeSelection(const FArcTacticsEditorSelection& Selection, bool bScrollIntoView);

	FArcTacticsEditorSelection GetAddParent(EArcTacticsEditorItemKind Kind) const;
	TSharedRef<SWidget> CreateReplaceTypePicker(FArcTacticsEditorSelection Item);
	void OpenTypePicker(EArcTacticsEditorItemKind Kind, FArcTacticsEditorSelection Target, bool bReplace);

	void RequestRename();
	bool CanRename() const;
	void DeleteSelection();
	bool CanDeleteSelection() const;
	void DuplicateSelection();
	bool CanDuplicateSelection() const;
	void CopySelection();
	bool CanCopySelection() const;
	void PasteSelection();
	bool CanPasteSelection() const;
	void MoveSelection(bool bUp);
	bool CanMoveSelection(bool bUp) const;
	bool FindMoveTarget(bool bUp, FArcTacticsEditorSelection& OutTarget) const;
	void OpenAddPicker(EArcTacticsEditorItemKind Kind);
	bool CanAddStep() const;
	bool CanAddActiveStep() const;
	bool CanAddConsideration() const;
	void OpenReplacePicker();
	bool CanReplaceSelection() const;

	TSharedPtr<FArcTacticsViewItem> FindItem(const FArcTacticsEditorSelection& Selection) const;
	TSharedPtr<FArcTacticsViewItem> FindItemRecursive(
		const TArray<TSharedPtr<FArcTacticsViewItem>>& Items,
		const FArcTacticsEditorSelection& Selection) const;
	bool FindItemPathRecursive(
		const TArray<TSharedPtr<FArcTacticsViewItem>>& Items,
		const FArcTacticsEditorSelection& Selection,
		TArray<TSharedPtr<FArcTacticsViewItem>>& OutPath) const;

	TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<STreeView<TSharedPtr<FArcTacticsViewItem>>> TreeView;
	TSharedPtr<SSearchBox> SearchBox;
	TArray<TSharedPtr<FArcTacticsViewItem>> RootItems;
	TSet<FArcTacticsViewItemKey> ExpansionKeys;
	TSet<FArcTacticsViewItemKey> UnfilteredExpansionKeys;
	FString SearchText;
	bool bFiltering = false;
	bool bSynchronizingSelection = false;
};
