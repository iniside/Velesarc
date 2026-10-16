// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlState.h"
#include "ISourceControlProvider.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
// EColumnSortMode / EColumnSortPriority (namespaced enums) used by the sort state + handlers below.
#include "Widgets/Views/SHeaderRow.h"
// Complete types required at the ISourceControlOperation::Create<FLorePush/FLoreUnstage>() call sites.
#include "ArcLoreSourceControlOperations.h"
// Shared EArcLoreSortKey + sort comparator/SortFileItems (the alias below names the enum).
#include "Panel/ArcLoreAssetDisplay.h"

class FArcLoreSourceControlProvider;
class FArcLoreSourceControlRevision;
class FLoreRevisionLog;
class FLoreRevisionFiles;
class FLoreListLocks;
class SEditableTextBox;
class SMultiLineEditableTextBox;
class STextBlock;
class SHeaderRow;
class ITableRow;
template <typename OptionType> class SComboBox;
class SWidgetSwitcher;

/** One row in a panel file list: holds the shared source-control state for that path. */
struct FArcLorePanelFileItem
{
	FSourceControlStateRef State;

	/** Transient display fields — lazily resolved once on first row generation
	 *  (see ResolveDisplayInfo): a useful name (actor label / asset short name)
	 *  instead of the raw OFPA GUID filename, the owning map / package folder,
	 *  and the full on-disk path kept as the tooltip. */
	FString CachedPrimary;
	FString CachedSecondary;
	FString CachedTooltip;
	bool bResolved = false;

	/** Transient (display only): asset class short name (e.g. "WidgetBlueprint"),
	 *  resolved no-load via ArcLoreAssetDisplay::ResolveAssetTypeName, cached once. */
	FString CachedType;
	bool bTypeResolved = false;

	explicit FArcLorePanelFileItem(const FSourceControlStateRef& InState)
		: State(InState)
	{
	}
};

/** One node in the Outgoing tree. A node is a discriminated union of exactly one of
 *  two kinds:
 *    - a COMMIT node (top level): wraps one committed-but-unpushed local revision;
 *    - a FILE node (child): wraps one file changed in that commit.
 *  The kind is decided by which member is set at construction (Commit valid => commit
 *  node; otherwise file node). Unlike the lazy Branch History tree, the whole tree is
 *  built eagerly in OnUnpushedComplete (the worker already enumerated every commit's
 *  files), so Children is fully populated up front and there is no placeholder. */
struct FArcLoreOutgoingNode
{
	/** COMMIT-node payload (valid only on commit nodes). */
	TSharedPtr<FArcLoreUnpushedCommit> Commit;

	/** FILE-node payload (valid only on file nodes). Carries AbsPath/Cached* used by
	 *  the friendly-name resolution (same fields the flat Outgoing rows used). */
	TSharedPtr<FArcLoreUnpushedFile> File;

	/** Child file nodes of a commit node (fully populated when the tree is built). */
	TArray<TSharedPtr<FArcLoreOutgoingNode>> Children;

	/** True when this node represents a commit (top-level row). */
	bool IsCommit() const { return Commit.IsValid(); }

	/** True when this node represents a changed file (child row). */
	bool IsFile() const { return File.IsValid(); }
};

/** Body layout for the panel. LegacyTabbed = the full tabbed panel (Changes/History/
 *  Repositories/Locks). FourSection = a single Changes view split into four sections
 *  (Dirty / Uncontrolled / Staged / Outgoing) with no internal tab bar. */
enum class EArcLoreChangesLayout : uint8
{
	LegacyTabbed,
	FourSection
};

/**
 * Dockable Lore source-control panel: a shared header (branch/remote/identity/root) and a
 * local-commits-to-push summary row, then a tab bar driving an SWidgetSwitcher with three
 * tabs — Changes (toolbar + Dirty + Staged), History (branch revision log), and
 * Repositories (branches / instances / remote repos).
 *
 * In FourSection layout mode the tab bar/switcher are skipped and the body is a single
 * 2x2 grid (Dirty / Uncontrolled on top, Staged / Outgoing below) plus the commit block —
 * reusing all the same actions, list views and row generators.
 */
class SArcLoreSourceControlPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcLoreSourceControlPanel)
		: _LayoutMode(EArcLoreChangesLayout::LegacyTabbed)
	{}
		SLATE_ARGUMENT(EArcLoreChangesLayout, LayoutMode)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcLoreSourceControlPanel() override;

	/** Sort key shared by every file list (folds the per-list Name/Path/Type columns).
	 *  Aliased to the canonical enum in ArcLoreAssetDisplay so both panels share one copy. */
	using EArcLoreSortKey = ArcLoreAssetDisplay::EArcLoreSortKey;

private:
	/** Resolve the ArcLore-specific provider from the active source control module. */
	FArcLoreSourceControlProvider& GetProvider() const;

	/** Re-bin the cached states into the Dirty/Staged arrays and refresh both list views. */
	void RefreshFileLists();

	/** Dispatch the repo-wide async FUpdateStatus (repo-root sentinel) that discovers all
	 *  dirty + untracked files project-wide, then refresh the Outgoing list. Guarded on
	 *  provider availability; completion (OnOperationComplete) re-bins the lists. */
	void RefreshAllRepoWide();

	/** Delegate target: provider broadcast that the cached states changed. */
	void OnSourceControlStateChanged();

	/** Completion delegate for every async op issued by this panel. */
	void OnOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/* ---- Header / summary text ---- */
	FText GetHeaderText() const;
	FText GetUnpushedSummaryText() const;
	FText GetIdentityText() const;

	/* ---- Login / Logout ---- */
	FReply OnLoginInteractiveClicked();
	void OnInteractiveLoginComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<class FLoreLoginInteractive> InOp);
	FReply OnLoginClicked();
	void OnLoginComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreLoginWithToken> InOp);
	FReply OnLogoutClicked();

	/** True when the panel is idle and the provider has NO resolved identity (Login enabled). */
	bool CanLogin() const;

	/** True when the panel is idle and the provider has a resolved identity (Logout enabled). */
	bool CanLogout() const;

	/* ---- Tab bar ---- */

	/** Tab-button handler: switch the body switcher imperatively to TabIndex. */
	FReply OnTabClicked(int32 TabIndex);

	/** Active-tab visual cue: a brighter tint for the button whose index is active. */
	FSlateColor GetTabButtonColor(int32 TabIndex) const;

	/* ---- Toolbar actions ---- */
	FReply OnRefreshClicked();
	FReply OnStageClicked();
	FReply OnUnstageClicked();

	/** Toolbar handler: unstage staged-Added cached entries whose .uasset is gone from disk
	 *  (mark-for-add then deleted), then evict them from the provider cache via a dedicated
	 *  completion delegate so they disappear instead of relocating to the Uncontrolled bin.
	 *  Staged deletes-on-disk are left untouched (a real pending delete). */
	FReply OnCleanStaleStagedClicked();

	FReply OnSyncClicked();
	FReply OnRevertClicked();
	FReply OnCommitClicked();
	FReply OnCommitAndPushClicked();
	FReply OnPushClicked();

	/** Shared confirm-and-dispatch used by both OnRevertClicked and OnRevertSelectedFromMenu. */
	void DoRevertSelected();

	/** Context-menu action entry points for the Dirty list. */
	void OnRevertSelectedFromMenu();
	void OnResolveSelectedFromMenu();

	/** Context-menu Lock/Unlock entry points. Operate on the list the menu was opened on. */
	void OnLockSelected(TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView);
	void OnUnlockSelected(TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView);

	/** Lock enable guard: idle and at least one selected item is NOT already locked-by-me. */
	bool CanLockSelected(TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView) const;

	/** Unlock enable guard: idle and at least one selected item IS locked-by-me. */
	bool CanUnlockSelected(TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView) const;

	/** Completion delegate for the commit half of Commit & Push; on success chains FLorePush on the committed files. */
	void OnCommitThenPushComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TArray<FString> CommittedFiles);

	/* ---- Toolbar enable guards ---- */
	bool IsIdle() const;
	bool CanStage() const;
	bool CanUnstage() const;
	bool CanRevertSelected() const;
	bool CanResolveSelected() const;
	bool CanCommit() const;
	bool CanPush() const;

	/** Context-menu factory for the Dirty list (Revert/Resolve + Lock/Unlock). */
	TSharedPtr<SWidget> OnDirtyContextMenuOpening();

	/** Context-menu factory for the Staged list (Lock/Unlock). */
	TSharedPtr<SWidget> OnStagedContextMenuOpening();

	/** Shared Lock/Unlock menu entries appended onto a list's context menu. */
	void AddLockMenuEntries(class FMenuBuilder& MenuBuilder, TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView);

	/** Shared Open / Browse entries prepended onto a list's context menu. Labels are picked
	 *  by ArcLoreAssetDisplay::ClassifyFile (Open Asset / Browse to Asset for a plain asset,
	 *  Open Map / Browse to Map for an external actor/object). No-op for an empty path. */
	void AddOpenBrowseEntries(class FMenuBuilder& MenuBuilder, const FString& AbsPath);

	/** Context-menu factory for the Outgoing tree: Open / Browse for a selected FILE node
	 *  (commit-header nodes get no entries). */
	TSharedPtr<SWidget> OnOutgoingContextMenuOpening();

	/** Compose the final commit message: Summary alone, or "Summary\n\nDescription" (git subject + body). */
	FText BuildCommitMessage() const;

	/* ---- List view row generation ---- */
	TSharedRef<ITableRow> OnGenerateDirtyRow(TSharedPtr<FArcLorePanelFileItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateStagedRow(TSharedPtr<FArcLorePanelFileItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateUncontrolledRow(TSharedPtr<FArcLorePanelFileItem> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Context-menu factory for the Uncontrolled list (Stage + Lock/Unlock). */
	TSharedPtr<SWidget> OnUncontrolledContextMenuOpening();

	/** Build the FourSection-mode ChildSlot (single 2x2 Changes grid + commit block). */
	void ConstructFourSectionBody();

	/** Row generator for the Outgoing tree: switches the row widget on node kind
	 *  (commit header vs changed file). */
	TSharedRef<ITableRow> OnGenerateOutgoingRow(TSharedPtr<FArcLoreOutgoingNode> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Children accessor for the Outgoing tree (returns the node's populated Children). */
	void OnGetOutgoingChildren(TSharedPtr<FArcLoreOutgoingNode> InItem, TArray<TSharedPtr<FArcLoreOutgoingNode>>& OutChildren);

	/** Build a fresh column header row for a file list. Non-const so Step 3 can bind a
	 *  non-const OnSort handler onto its columns. */
	TSharedRef<SHeaderRow> MakeFileHeaderRow();

	/** Build a fresh column header row for the Outgoing (committed-but-unpushed) tree:
	 *  Name (commit "#N message" / file name) + Type + Path (commit date / file folder). */
	TSharedRef<SHeaderRow> MakeOutgoingHeaderRow();

	/** Build a fresh column header row for the History selected-revision FILE sub-list
	 *  (Icon / Name / Type / Path). */
	TSharedRef<SHeaderRow> MakeRevisionFileHeaderRow();

	/** Build the toolbar sort controls (sort-key dropdown + ascending/descending toggle),
	 *  the second sort entry-point alongside the clickable column headers. Inserted into
	 *  whichever layout toolbar builds (only one is live per LayoutMode), so it assigns the
	 *  single SortKeyCombo member shared with SyncSortCombo. */
	TSharedRef<SWidget> MakeSortControls();

	/* ---- Column sorting (shared single-column state across every file list) ---- */

	/** Map a clickable column id to its sort key. Name/Type/Path columns of both the
	 *  changes lists and the outgoing/revision lists fold onto the same three keys;
	 *  any non-sortable column (Icon/Lock) falls through to Name. */
	EArcLoreSortKey KeyForColumn(const FName& Col) const;

	/** SHeaderRow SortMode attribute source: the active mode when Col is the sorted
	 *  column, EColumnSortMode::None otherwise (drives the arrow indicator). */
	EColumnSortMode::Type GetSortModeForColumn(FName Col) const;

	/** SHeaderRow OnSort handler: adopt the clicked column's key + the requested mode,
	 *  then re-sort every list. */
	void OnSortColumnHeader(EColumnSortPriority::Type SortPriority, const FName& Col, EColumnSortMode::Type NewMode);

	/** Re-sort every file list/tree and refresh each valid view. Used by header clicks
	 *  (and, in Step 4, the toolbar sort dropdown). */
	void ApplySortToAllLists();

	/** Sort the changes-section lists (Dirty/Staged/Uncontrolled) in place + refresh. */
	void SortChangesLists();

	/** Sort the History selected-revision file list in place + refresh. */
	void SortRevisionFiles();

	/** Sort the file children within each Outgoing commit node in place + refresh. */
	void SortOutgoingTree();

	/** Map a sort-key dropdown label ("Name"/"Path"/"Type") to its enum; unknown ⇒ Name. */
	EArcLoreSortKey SortKeyFromLabel(const FString& Label) const;

	/** Map a sort key back to its dropdown label ("Name"/"Path"/"Type"). */
	FString LabelForSortKey(EArcLoreSortKey Key) const;

	/** Push the active SortKey into the toolbar dropdown so a column-header click keeps the
	 *  dropdown selection in sync. Null-guarded (the combo lives only on the built toolbar). */
	void SyncSortCombo();

	/** Direction-toggle button handler: flip SortMode Ascending<->Descending (None => Ascending),
	 *  then re-sort every list. */
	FReply OnToggleSortDirectionClicked();

	/* ---- Outgoing (committed-but-unpushed) ---- */

	/** Issue the async FLoreListUnpushed op to (re)populate the Outgoing list. */
	void RefreshUnpushed();

	/** Completion delegate for FLoreListUnpushed: rebuild the Outgoing tree (commit nodes +
	 *  file children), expand all commits, and refresh the tree view. */
	void OnUnpushedComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreListUnpushed> InOp);

	/* ---- Unpushed history rewrite (DESTRUCTIVE — confirm-gated) ---- */

	/** Button handler: confirm, then dispatch FLoreUndoLastCommit (resets HEAD to its parent). */
	FReply OnUndoLastCommitClicked();

	/** Button handler: confirm, then dispatch FLoreRemoveFromOutgoing on the selected Outgoing file. */
	FReply OnRemoveFromOutgoingClicked();

	/** Enable guard: idle and the local tip is ahead of the remote (something to undo). */
	bool CanUndoLastCommit() const;

	/** Enable guard: idle, local tip ahead, and exactly one Outgoing file selected. */
	bool CanRemoveFromOutgoing() const;

	/** Button handler: open a multi-line message dialog, then dispatch FLoreAmendLastCommit. */
	FReply OnAmendLastCommitClicked();

	/** Enable guard: idle and the local tip is ahead of the remote (something to amend). */
	bool CanAmendLastCommit() const;

	/* ---- History (branch revision log) ---- */

	/** Issue the async FLoreRevisionLog op to (re)populate the History list. */
	void RefreshHistory();

	/** Completion delegate for FLoreRevisionLog: copy Revisions into HistoryItems and refresh. */
	void OnHistoryComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRevisionLog> InOp);

	/** Row generator for the History list view. */
	TSharedRef<ITableRow> OnGenerateHistoryRow(TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** History-list selection handler: when a revision is selected, list its changed
	 *  files (FLoreRevisionFiles) into the sub-list; on clear, empty the sub-list. */
	void OnHistorySelectionChanged(TSharedPtr<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> InItem, ESelectInfo::Type InSelectInfo);

	/** Completion delegate for FLoreRevisionFiles: rebuild RevisionFileItems and refresh. */
	void OnRevisionFilesComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRevisionFiles> InOp);

	/** Row generator for the selected-revision file sub-list (reuses the Outgoing row). */
	TSharedRef<ITableRow> OnGenerateRevisionFileRow(TSharedPtr<FArcLoreUnpushedFile> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/* ---- Repositories (branches / instances / remote repos) ---- */

	/** Issue the LOCAL async FLoreRepositoryView op to (re)populate the Branches/Instances lists. */
	void RefreshRepositories();

	/** Completion delegate for FLoreRepositoryView: copy Branches/Instances and refresh. */
	void OnRepositoryViewComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRepositoryView> InOp);

	/** Button handler: dispatch the network-blocking FLoreListRemoteRepos op (never auto-dispatched). */
	FReply OnListRemoteReposClicked();

	/** Completion delegate for FLoreListRemoteRepos: copy RemoteRepos, refresh, clear the latch. */
	void OnRemoteReposComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreListRemoteRepos> InOp);

	/** Button handler: switch the working copy to the selected branch. Shows a YesNoCancel
	 *  dialog when staged or local changes are present (Yes = keep, No = discard/reset). */
	FReply OnSwitchBranchClicked();

	/** Completion delegate for FLoreBranchSwitch: clears latch, shows error dialog on
	 *  failure, refreshes branches and file lists. */
	void OnBranchSwitchComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchSwitch> InOp);

	/** Enable guard for the Switch button: idle, list valid, one row selected, and it is
	 *  not the current branch. */
	bool CanSwitchBranch() const;

	/** Button handler: opens a modal name+category dialog, then dispatches FLoreBranchCreate.
	 *  On success, prompts whether to switch to the new branch. */
	FReply OnCreateBranchClicked();

	/** Completion delegate for FLoreBranchCreate: clears latch, offers a switch-to-it
	 *  follow-up dialog on success, refreshes repositories. */
	void OnBranchCreateComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchCreate> InOp);

	/** Row generators for the three repository list views. */
	TSharedRef<ITableRow> OnGenerateBranchRow(TSharedPtr<FArcLoreBranchEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateInstanceRow(TSharedPtr<FArcLoreInstanceEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateRemoteRepoRow(TSharedPtr<FArcLoreRepoEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/* ---- Locks (branch-wide lock browser) ---- */

	/** Issue the async FLoreListLocks op to (re)populate the Locks list. */
	void RefreshLocks();

	/** Completion delegate for FLoreListLocks: rebuild LockItems and refresh the list view. */
	void OnLocksComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreListLocks> InOp);

	/** Row generator for the Locks list view. */
	TSharedRef<ITableRow> OnGenerateLockRow(TSharedPtr<FArcLoreLockEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Button handler: dispatch FLoreReleaseLock on the selected lock (explicit Unlock without pushing). */
	FReply OnReleaseSelectedLockClicked();

	/** Enable guard: idle and exactly one lock row is selected. */
	bool CanReleaseSelectedLock() const;

	/** Collect the source-control filenames currently selected in a list view. */
	TArray<FString> GetSelectedFilenames(const TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>>& InListView) const;

	/** Collect every filename currently in the Staged section. */
	TArray<FString> GetAllStagedFilenames() const;

	/** Handle to the provider's state-changed broadcast (unregistered in the dtor). */
	FDelegateHandle StateChangedHandle;

	/** Backing arrays for the two list views. */
	TArray<TSharedPtr<FArcLorePanelFileItem>> DirtyItems;
	TArray<TSharedPtr<FArcLorePanelFileItem>> StagedItems;

	/** The two file list views. */
	TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> DirtyListView;
	TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> StagedListView;

	/** Backing array + list view for the Uncontrolled (on-disk, not in Lore) section.
	 *  Populated only in FourSection layout mode; in LegacyTabbed mode NotControlled
	 *  files bin into Dirty so this stays empty. */
	TArray<TSharedPtr<FArcLorePanelFileItem>> UncontrolledItems;
	TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> UncontrolledListView;

	/** Backing roots + tree view for the Outgoing (committed-but-unpushed) section,
	 *  grouped by commit (commit node -> its changed-file children). */
	TArray<TSharedPtr<FArcLoreOutgoingNode>> OutgoingRoots;
	TSharedPtr<STreeView<TSharedPtr<FArcLoreOutgoingNode>>> OutgoingTreeView;

	/** Backing array + list view for the branch revision-log (History) section. */
	TArray<TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>> HistoryItems;
	TSharedPtr<SListView<TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>>> HistoryListView;

	/** Backing array + list view for the selected-revision file sub-list (under History),
	 *  plus the caption above it ("Files in revision #N"). */
	TArray<TSharedPtr<FArcLoreUnpushedFile>> RevisionFileItems;
	TSharedPtr<SListView<TSharedPtr<FArcLoreUnpushedFile>>> RevisionFilesView;
	TSharedPtr<class STextBlock> RevisionFilesHeader;

	/** Read-only multiline box showing the full commit message of the selected revision. */
	TSharedPtr<SMultiLineEditableTextBox> RevisionMessageBox;

	/** Backing arrays + list views for the Repositories section (branches / instances / remote repos). */
	TArray<TSharedPtr<FArcLoreBranchEntry>> BranchItems;
	TSharedPtr<SListView<TSharedPtr<FArcLoreBranchEntry>>> BranchListView;

	TArray<TSharedPtr<FArcLoreInstanceEntry>> InstanceItems;
	TSharedPtr<SListView<TSharedPtr<FArcLoreInstanceEntry>>> InstanceListView;

	/** Remote repos are populated ONLY by the explicit "List remote repositories" button. */
	TArray<TSharedPtr<FArcLoreRepoEntry>> RemoteRepoItems;
	TSharedPtr<SListView<TSharedPtr<FArcLoreRepoEntry>>> RemoteRepoListView;

	/** Backing array + list view for the Locks (branch-wide lock browser) section. */
	TArray<TSharedPtr<FArcLoreLockEntry>> LockItems;
	TSharedPtr<SListView<TSharedPtr<FArcLoreLockEntry>>> LockListView;

	/** Commit summary (subject line — required) and its single-line box. */
	TSharedPtr<SEditableTextBox> CommitSummaryBox;
	FText CommitSummary;

	/** Commit description (body — optional) and its multiline box. */
	TSharedPtr<SMultiLineEditableTextBox> CommitDescriptionBox;
	FText CommitDescription;

	/** Token-type combo-box backing data. */
	TArray<TSharedPtr<FString>> TokenTypeOptions;
	TSharedPtr<FString> SelectedTokenType;

	/** True while an async op is in flight; disables the toolbar. */
	bool bOperationInProgress = false;

	/** True while the interactive browser-login op is in flight — used only to show
	 *  the "Signing in…" identity text; does NOT replace bOperationInProgress as gate. */
	bool bIsLoggingIn = false;

	/** Body switcher driving the three tabs (Changes / History / Repositories). */
	TSharedPtr<class SWidgetSwitcher> TabSwitcher;

	/** Currently active tab index (0 = Changes, 1 = History, 2 = Repositories, 3 = Locks). */
	int32 ActiveTabIndex = 0;

	/** Body layout: set first thing in Construct from InArgs._LayoutMode. */
	EArcLoreChangesLayout LayoutMode = EArcLoreChangesLayout::LegacyTabbed;

	/** Active sort key + direction shared across all lists; one source of truth read by
	 *  the column headers (and the Step 4 dropdown). */
	EArcLoreSortKey SortKey = EArcLoreSortKey::Name;
	EColumnSortMode::Type SortMode = EColumnSortMode::Ascending;

	/** Toolbar sort-key dropdown backing data: "Name" / "Path" / "Type" (filled once in
	 *  Construct). The combo + direction toggle are the second sort entry-point alongside
	 *  the clickable column headers; both drive the shared SortKey/SortMode above. Only one
	 *  toolbar is built per LayoutMode, so a single combo member assigned by whichever build
	 *  path runs is correct. */
	TArray<TSharedPtr<FString>> SortKeyOptions;
	TSharedPtr<FString> SelectedSortKey;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> SortKeyCombo;
};
