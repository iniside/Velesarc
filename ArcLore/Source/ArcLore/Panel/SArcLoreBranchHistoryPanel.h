// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
// Complete types required at the ISourceControlOperation::Create<FLoreRevisionLog/FLoreRevisionFiles>()
// call sites and for FArcLoreUnpushedFile (the changed-file node payload).
#include "ArcLoreSourceControlOperations.h"
#include "ArcLoreSourceControlRevision.h"
// Shared EArcLoreSortKey enum + SortFileItems template consumed by the toolbar sort members below.
#include "Panel/ArcLoreAssetDisplay.h"
// EColumnSortMode (namespaced enum) typed by the sort state members below.
#include "Widgets/Views/SHeaderRow.h"

class FArcLoreSourceControlProvider;
class FLoreRevisionLog;
class FLoreRevisionFiles;
class SHeaderRow;
class ITableRow;
class SMultiLineEditableTextBox;
template <typename OptionType> class SComboBox;

/**
 * One node in the Branch History tree. A node is a discriminated union of exactly
 * one of two kinds:
 *   - a REVISION node (top level): wraps a branch revision from the commit log;
 *   - a FILE node (child): wraps one file changed in the parent revision.
 * The kind is decided by which member is set at construction (Revision valid =>
 * revision node; otherwise file node). Revision nodes carry lazily-populated
 * Children (the changed files), guarded by bChildrenRequested so the file query is
 * issued at most once per row.
 */
struct FArcLoreHistoryTreeItem
{
	/** REVISION-node payload (valid only on revision nodes). */
	TSharedPtr<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> Revision;

	/** FILE-node payload (meaningful only on file nodes). */
	FArcLoreUnpushedFile File;

	/** Child file nodes of a revision node (populated lazily on first expand). */
	TArray<TSharedPtr<FArcLoreHistoryTreeItem>> Children;

	/** True once the lazy FLoreRevisionFiles query has been issued for this revision
	 *  node — prevents a second issue on rapid expand/collapse toggling. */
	bool bChildrenRequested = false;

	/** True when this is a synthetic placeholder child ("Loading changed files…").
	 *  A revision node carries exactly one placeholder until its real file children
	 *  arrive, so STreeView draws the expander arrow (it only does so when
	 *  OnGetChildren yields at least one child). The placeholder is dropped when
	 *  OnRevisionFilesComplete resets Children to the real file nodes. */
	bool bPlaceholder = false;

	/** True when this node represents a revision (top-level row). */
	bool IsRevision() const { return Revision.IsValid(); }

	/** True when this node represents a changed file (child row).
	 *  A placeholder is neither a revision nor a file. */
	bool IsFile() const { return !Revision.IsValid() && !bPlaceholder; }

	/** True when this node is the synthetic "loading" placeholder child. */
	bool IsPlaceholder() const { return bPlaceholder; }

	/** Build a revision node from a log entry. */
	static TSharedRef<FArcLoreHistoryTreeItem> MakeRevision(const TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>& InRevision)
	{
		TSharedRef<FArcLoreHistoryTreeItem> Node = MakeShared<FArcLoreHistoryTreeItem>();
		Node->Revision = InRevision;
		return Node;
	}

	/** Build a changed-file node from a diff entry. */
	static TSharedRef<FArcLoreHistoryTreeItem> MakeFile(const FArcLoreUnpushedFile& InFile)
	{
		TSharedRef<FArcLoreHistoryTreeItem> Node = MakeShared<FArcLoreHistoryTreeItem>();
		Node->File = InFile;
		return Node;
	}

	/** Build the synthetic placeholder child that forces the expander arrow to appear
	 *  on a revision node before its real file children have been queried. Leaves
	 *  Revision invalid so IsRevision()/IsFile() both return false for it. */
	static TSharedRef<FArcLoreHistoryTreeItem> MakePlaceholder()
	{
		TSharedRef<FArcLoreHistoryTreeItem> Node = MakeShared<FArcLoreHistoryTreeItem>();
		Node->bPlaceholder = true;
		return Node;
	}
};

/**
 * Dedicated "Branch History" tab: a chronological tree of branch revisions (newest
 * first) that expands lazily to the files each revision changed. A toolbar offers
 * Refresh / Sync to Latest / Check in Changes; revision rows offer a confirm-gated
 * "Sync to this revision (Rewind)" context action; changed-file rows offer
 * "Open in Content Browser". Mirrors SArcLoreSourceControlPanel's provider/async
 * idioms (GetProvider, bound-op completion delegates, bOperationInProgress guard,
 * OnSourceControlStateChanged binding).
 */
class SArcLoreBranchHistoryPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcLoreBranchHistoryPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcLoreBranchHistoryPanel() override;

private:
	/** Resolve the ArcLore-specific provider from the active source control module. */
	FArcLoreSourceControlProvider& GetProvider() const;

	/** Delegate target: provider broadcast that the cached states changed. */
	void OnSourceControlStateChanged();

	/* ---- Tree population (top-level revisions) ---- */

	/** Issue the async FLoreRevisionLog op to (re)populate the top-level revision rows. */
	void RefreshHistory();

	/** Completion delegate for FLoreRevisionLog: cache the revisions and rebuild via the filter. */
	void OnHistoryComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRevisionLog> InOp);

	/** Rebuild RootItems from LoadedRevisions, applying the current date-range filter and
	 *  appending one placeholder child per surviving revision (so its expander appears).
	 *  Newest-first order is preserved (LoadedRevisions is already newest-first). */
	void RebuildRootsFromFilter();

	/* ---- Date-range filter (toolbar) ---- */

	/** Compute the inclusive lower-bound date for the currently selected range, or return
	 *  false for "All time" (no lower bound — keep every revision). */
	bool TryGetFilterCutoff(FDateTime& OutCutoff) const;

	/** Combo row generator: one STextBlock per range option. */
	TSharedRef<SWidget> OnGenerateFilterOption(TSharedPtr<FString> InOption) const;

	/** Combo selection change: store the new range and rebuild the roots. */
	void OnFilterSelectionChanged(TSharedPtr<FString> InNewSelection, ESelectInfo::Type InSelectInfo);

	/** Current combo-button label (the selected range). */
	FText GetSelectedFilterLabel() const;

	/* ---- Sort (toolbar dropdown + direction toggle, over a revision's file children) ---- */

	/** Build the toolbar "Sort:" caption + key dropdown (Name/Path/Type) + asc/desc toggle.
	 *  Mirrors SArcLoreSourceControlPanel::MakeSortControls (this panel has no column-header
	 *  sort, so the dropdown + toggle are the only sort entry points). */
	TSharedRef<SWidget> MakeSortControls();

	/** Map a dropdown label to the sort key (unknown labels fold onto Name). */
	ArcLoreAssetDisplay::EArcLoreSortKey SortKeyFromLabel(const FString& Label) const;

	/** Map the active sort key to its dropdown label. */
	FString LabelForSortKey(ArcLoreAssetDisplay::EArcLoreSortKey Key) const;

	/** Dropdown selection change: adopt the new key and re-sort. Guards ESelectInfo::Direct
	 *  so a programmatic SetSelectedItem never re-triggers a sort. */
	void OnSortKeySelectionChanged(TSharedPtr<FString> InNewSelection, ESelectInfo::Type InSelectInfo);

	/** Direction-toggle button: flip ascending/descending and re-sort. */
	FReply OnToggleSortDirectionClicked();

	/** Sort one revision node's file children in place by the active key/mode. Skips the
	 *  synthetic "loading" placeholder (only sorts when Children hold real file nodes). */
	void SortFileChildrenOf(FArcLoreHistoryTreeItem& RevisionNode);

	/** Apply the active sort to every revision's file children, then refresh the tree. */
	void SortRevisionFileChildren();

	/* ---- Tree wiring ---- */

	/** Children accessor for the tree (returns the node's already-populated Children). */
	void OnGetChildren(TSharedPtr<FArcLoreHistoryTreeItem> InItem, TArray<TSharedPtr<FArcLoreHistoryTreeItem>>& OutChildren);

	/** Row generator: switches the row widget on node kind (revision vs file). */
	TSharedRef<ITableRow> OnGenerateRow(TSharedPtr<FArcLoreHistoryTreeItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Lazy child load: on first expand of a revision row, issue FLoreRevisionFiles. */
	void OnExpansionChanged(TSharedPtr<FArcLoreHistoryTreeItem> InItem, bool bExpanded);

	/** Selection change: show the selected revision's full commit message in the bottom
	 *  description box (cleared for file/placeholder rows or an empty selection). */
	void OnTreeSelectionChanged(TSharedPtr<FArcLoreHistoryTreeItem> InItem, ESelectInfo::Type InSelectInfo);

	/** Completion delegate for FLoreRevisionFiles: build the target revision's file children
	 *  and refresh. The target node is captured weakly and re-validated. */
	void OnRevisionFilesComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRevisionFiles> InOp, TWeakPtr<FArcLoreHistoryTreeItem> TargetItem);

	/** Build a fresh column header row for the revision/file tree. */
	TSharedRef<SHeaderRow> MakeHeaderRow() const;

	/* ---- Context menu ---- */

	/** Context-menu factory: revision rows get Rewind; file rows get kind-correct Open/Browse. */
	TSharedPtr<SWidget> OnContextMenuOpening();

	/** Revision context action: confirm-gated sync (rewind) of the working copy to a revision. */
	void OnRewindToRevision(TSharedPtr<FArcLoreHistoryTreeItem> InItem);

	/* ---- Toolbar actions ---- */

	/** Toolbar: re-issue the revision log. */
	FReply OnRefreshClicked();

	/** Toolbar: confirm-gated sync to the branch tip (FSync, no target revision). */
	FReply OnSyncToLatestClicked();

	/** Toolbar: invoke the existing Lore source-control panel tab (commit UX). */
	FReply OnCheckInChangesClicked();

	/** Completion delegate for FSync (both Rewind and Sync-to-Latest): surface the result. */
	void OnSyncComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/* ---- Shared ---- */

	/** Toolbar/context enable guard: no async op currently in flight. */
	bool IsIdle() const;

	/** Handle to the provider's state-changed broadcast (unregistered in the dtor). */
	FDelegateHandle StateChangedHandle;

	/** All revisions returned by the last log query (newest first). RootItems is derived
	 *  from this by applying the date-range filter; this is the unfiltered source of truth. */
	TArray<TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>> LoadedRevisions;

	/** Top-level revision nodes (newest first); the tree's item source. Derived from
	 *  LoadedRevisions via RebuildRootsFromFilter. */
	TArray<TSharedPtr<FArcLoreHistoryTreeItem>> RootItems;

	/** The revision/file tree view. */
	TSharedPtr<STreeView<TSharedPtr<FArcLoreHistoryTreeItem>>> TreeView;

	/** Read-only box at the bottom showing the selected revision's FULL commit message
	 *  (the tree row shows only the first line / summary). */
	TSharedPtr<SMultiLineEditableTextBox> RevisionMessageBox;

	/* ---- Date-range filter state ---- */

	/** Selectable range labels (owned; the combo holds weak views into these). */
	TArray<TSharedPtr<FString>> FilterOptions;

	/** The currently selected range option (an element of FilterOptions; default "All time"). */
	TSharedPtr<FString> SelectedFilter;

	/** The range combo box in the toolbar. */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> FilterComboBox;

	/* ---- Sort state (applied to the file children within each revision) ---- */

	/** Active sort key (Name/Path/Type). */
	ArcLoreAssetDisplay::EArcLoreSortKey SortKey = ArcLoreAssetDisplay::EArcLoreSortKey::Name;

	/** Active sort direction. */
	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;

	/** Sort-key dropdown labels (owned; the combo holds weak views into these). */
	TArray<TSharedPtr<FString>> SortKeyOptions;

	/** The currently selected sort-key option (an element of SortKeyOptions; default "Name"). */
	TSharedPtr<FString> SelectedSortKey;

	/** The sort-key combo box in the toolbar. */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SortKeyCombo;

	/** True while an async op is in flight; disables the toolbar/context actions. */
	bool bOperationInProgress = false;
};
