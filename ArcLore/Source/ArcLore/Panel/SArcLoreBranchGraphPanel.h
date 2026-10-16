// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceControlProvider.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
// Complete types required at the ISourceControlOperation::Create<FLoreBranchGraph/…>() call
// sites and for FArcLoreCommitNode (the selected-node payload + context-menu actions).
#include "ArcLoreSourceControlOperations.h"
#include "Graph/ArcLoreGraphLayout.h"
#include "Graph/ArcLoreBranchTreeLayout.h"
#include "Graph/SArcLoreBranchTreeCanvas.h"

class FArcLoreSourceControlProvider;
class FLoreBranchGraph;
class FLoreBranchSwitch;
class FLoreBranchCreate;
class SArcLoreBranchGraph;
class SArcLoreBranchTreeCanvas;
class SMultiLineEditableTextBox;
class SWidget;
class FSync;
struct FArcLoreBranchTreeNode;

/**
 * Dedicated "Branch Explorer" tab host. The async FLoreBranchGraph op fetches the
 * cross-branch payload once; from it the panel builds TWO layouts: the primary
 * horizontal branch TREE (FArcLoreBranchTreeBuilder -> SArcLoreBranchTreeCanvas) and the
 * full commit DAG drill-down (FArcLoreGraphLayoutBuilder -> SArcLoreBranchGraph). The
 * branch tree is primary (top splitter slot); the commit graph plus a read-only detail
 * strip form the drill-down (bottom slot). A toolbar offers Refresh / "Include archived" /
 * "New Branch…". Selecting a branch blob updates the detail strip. Right-clicking a blob
 * offers Switch-to-branch (FLoreBranchSwitch, tri-state dirty guard; non-current only) and
 * Create-child-branch (FLoreBranchCreate from HEAD; enabled only on the current branch).
 * Rewind-to-a-specific-revision stays on the commit drill-down's own context menu. Mirrors
 * SArcLoreBranchHistoryPanel's provider/async idioms (GetProvider, bound-op completion
 * delegates, bOperationInProgress guard, OnSourceControlStateChanged binding).
 */
class SArcLoreBranchGraphPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcLoreBranchGraphPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SArcLoreBranchGraphPanel() override;

private:
	/** Resolve the ArcLore-specific provider from the active source control module. */
	FArcLoreSourceControlProvider& GetProvider() const;

	/** Delegate target: provider broadcast that the cached states changed. Re-refreshes
	 *  (Refresh self-guards against re-entrancy while an op is in flight). */
	void OnSourceControlStateChanged();

	/* ---- Graph fetch ---- */

	/** Issue the async FLoreBranchGraph op to (re)assemble the cross-branch DAG. Guards on
	 *  IsIdle() so a provider broadcast mid-fetch cannot stack a second op. */
	void Refresh();

	/** Completion delegate for FLoreBranchGraph (game thread): build the layout from the op's
	 *  branch/revision arrays and hand it to the graph widget. */
	void OnGraphFetched(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchGraph> InOp);

	/* ---- Branch-tree selection + actions (primary) ---- */

	/** Branch blob selected: refresh the detail strip from the branch node. No commit-layout rebuild. */
	void OnTreeBranchSelected(const FArcLoreBranchTreeNode& Node);

	/** Branch blob right-clicked: build + push the Switch / Create-child action menu at the cursor. */
	void OnTreeBranchContextMenu(const FArcLoreBranchTreeNode& Node);

	/** Whether the create-child action is allowed for a node (forks from HEAD ⇒ current branch only). */
	bool CanCreateChildOf(FArcLoreBranchTreeNode Node) const;

	/** Context action: create a child branch from HEAD (only meaningful when Node is the current branch). */
	void OnCreateChildBranch();

	/* ---- Commit drill-down selection ---- */

	/** Graph selection change: store the selected node and update the detail strip. */
	void OnCommitSelected(TSharedPtr<FArcLoreCommitNode> InNode);

	/* ---- Context menu (per-commit actions) ---- */

	/** Right-click row -> build the action menu from the currently selected node. Returns an
	 *  invalid ptr when nothing is selected (Slate suppresses an empty popup). */
	TSharedPtr<SWidget> BuildContextMenu();

	/** Context action: confirm-gated rewind of the working copy to the selected commit. */
	void OnRewindToRevision();

	/** Completion delegate for FSync (Rewind): surface the result, clear the in-flight flag. */
	void OnSyncComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult);

	/** Context action: switch to one of the selected commit's branch refs (tri-state dirty guard). */
	void OnSwitchToBranch(FString BranchName);

	/** Completion delegate for FLoreBranchSwitch: surface Lore's ErrorText verbatim on failure. */
	void OnBranchSwitchComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchSwitch> InOp);

	/* ---- Toolbar actions ---- */

	/** Toolbar: re-assemble the graph. */
	FReply OnRefreshClicked();

	/** Toolbar checkbox: archived-branch inclusion. */
	ECheckBoxState GetIncludeArchivedState() const;
	void OnIncludeArchivedChanged(ECheckBoxState InState);

	/** Toolbar: prompt for a name and create a branch from the current HEAD (FLoreBranchCreate). */
	FReply OnNewBranchClicked();

	/** Shared name-prompt + FLoreBranchCreate dispatch (HEAD fork). Used by the toolbar and the
	 *  branch-blob "Create child branch…" action. */
	void PromptAndCreateBranch();

	/** Completion delegate for FLoreBranchCreate: surface failure, refresh on success. */
	void OnBranchCreateComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchCreate> InOp);

	/* ---- Shared ---- */

	/** Toolbar/context enable guard: no async op currently in flight. (No conflict accessor
	 *  exists on the provider; mutations gate on IsIdle() only — conflicts surface in the
	 *  main Lore panel via the working-copy state cache.) */
	bool IsIdle() const;

	/** Handle to the provider's state-changed broadcast (unregistered in the dtor). */
	FDelegateHandle StateChangedHandle;

	/** Primary view: the horizontal branch-tree canvas (blobs = branches). */
	TSharedPtr<SArcLoreBranchTreeCanvas> TreeCanvas;

	/** Drill-down: the custom-painted commit-graph body (demoted from primary). */
	TSharedPtr<SArcLoreBranchGraph> GraphWidget;

	/** Last fetched branch list, kept for tree context-menu lookups (switch/create targets). */
	TArray<FArcLoreBranchInput> CachedBranches;

	/** Read-only strip showing the selected commit's message/author/date. */
	TSharedPtr<SMultiLineEditableTextBox> DetailBox;

	/** The currently selected commit node (drives the context menu + detail strip). */
	TSharedPtr<FArcLoreCommitNode> SelectedNode;

	/** True while an async op is in flight; disables the toolbar/context actions. */
	bool bOperationInProgress = false;

	/** Whether the fetch op asks lore_branch_list for archived branches too. */
	bool bIncludeArchived = false;
};
