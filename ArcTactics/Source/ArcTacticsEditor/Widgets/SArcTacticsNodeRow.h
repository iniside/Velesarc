// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTacticsEditorViewModel.h"
#include "Framework/SlateDelegates.h"
#include "Widgets/Views/STableRow.h"

class SBorder;
class SInlineEditableTextBlock;
class STableViewBase;
struct FArcTacticsViewItem;

class SArcTacticsNodeRow final : public STableRow<TSharedPtr<FArcTacticsViewItem>>
{
public:
	SLATE_BEGIN_ARGS(SArcTacticsNodeRow) {}
		SLATE_EVENT(FOnGetContent, OnGetTypePicker)
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		const TSharedRef<STableViewBase>& OwnerTable,
		TSharedPtr<FArcTacticsViewItem> InItem,
		TWeakPtr<FArcTacticsEditorViewModel> InViewModel);
	void RequestRename();

private:
	FText GetLabel() const;
	FText GetPlainLabel() const;
	FText GetRenameText() const;
	FText GetTooltip() const;
	const FSlateBrush* GetIcon() const;
	FSlateColor GetIconColor() const;
	const FSlateBrush* GetBackgroundBrush() const;
	EVisibility GetLabelVisibility() const;
	EVisibility GetRenameVisibility() const;
	EVisibility GetTypePickerVisibility() const;
	EVisibility GetAddGroupItemVisibility() const;
	FText GetAddGroupItemTooltip() const;
	TSharedRef<SWidget> CreateAddGroupItemPicker();
	EVisibility GetAddBranchNodeVisibility() const;
	EVisibility GetDeleteBranchNodeVisibility() const;
	bool CanAddBranchNode() const;
	bool CanDeleteBranchNode() const;
	FText GetDeleteBranchNodeTooltip() const;
	TSharedRef<SWidget> CreateAddStepPicker();
	TSharedRef<SWidget> CreateAddConsiderationPicker();
	FReply HandleDeleteBranchNodeClicked();
	bool IsNodeSelected() const;
	bool HasInvalidData() const;

	bool HandleVerifyName(const FText& Text, FText& OutError) const;
	void HandleNameCommitted(const FText& Text, ETextCommit::Type CommitType);
	FReply HandleDragDetected(const FGeometry& Geometry, const FPointerEvent& PointerEvent) const;
	void HandleDragLeave(const FDragDropEvent& DragDropEvent);
	TOptional<EItemDropZone> HandleCanAcceptDrop(
		const FDragDropEvent& DragDropEvent,
		EItemDropZone DropZone,
		TSharedPtr<FArcTacticsViewItem> TargetItem);
	FReply HandleAcceptDrop(
		const FDragDropEvent& DragDropEvent,
		EItemDropZone DropZone,
		TSharedPtr<FArcTacticsViewItem> TargetItem);
	TOptional<EArcTacticsEditorInsertLocation> ResolveDropLocation(
		EItemDropZone DropZone,
		const FArcTacticsEditorSelection& Source,
		const FArcTacticsEditorSelection& Target) const;
	void ClearDropState();

	TSharedPtr<FArcTacticsViewItem> Item;
	TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
	TSharedPtr<SInlineEditableTextBlock> RenameText;
	FOnGetContent OnGetTypePicker;
	mutable bool bRenaming = false;
	mutable bool bDropTarget = false;
	mutable bool bDropAllowed = false;
	mutable EArcTacticsEditorInsertLocation DropLocation = EArcTacticsEditorInsertLocation::After;
};
