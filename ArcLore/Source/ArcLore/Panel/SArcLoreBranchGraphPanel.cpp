// Copyright Epic Games, Inc. All Rights Reserved.

#include "Panel/SArcLoreBranchGraphPanel.h"

#include "Graph/SArcLoreBranchGraph.h"
#include "Graph/SArcLoreBranchTreeCanvas.h"
#include "Graph/ArcLoreGraphLayout.h"
#include "Graph/ArcLoreBranchTreeLayout.h"

#include "ArcLoreSourceControlModule.h"
#include "ArcLoreSourceControlProvider.h"
#include "ArcLoreSourceControlOperations.h"
#include "ISourceControlModule.h"
#include "SourceControlOperations.h"
#include "Modules/ModuleManager.h"
#include "RevisionControlStyle/RevisionControlStyle.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "SArcLoreBranchGraphPanel"

// Unique, file-scoped namespace (CLAUDE.md: cpp free functions / helpers in a unique named namespace).
namespace ArcLoreBranchGraphPanel_Internal
{
	/** Lore timestamps are milliseconds; divide by 1000 only when building an FDateTime. */
	FText FormatCommitDetail(const FArcLoreCommitNode& Node)
	{
		const FDateTime Date = FDateTime::FromUnixTimestamp(Node.TimestampMs / 1000);

		// Compose a compact "message / author / date" block for the read-only detail strip.
		return FText::Format(
			LOCTEXT("CommitDetailFormat", "{0}\n\nAuthor: {1}\nDate: {2}\nRevision: {3}"),
			FText::FromString(Node.Message),
			FText::FromString(Node.Author),
			FText::AsDateTime(Date),
			FText::FromString(Node.Hash));
	}

	/** Short (12-char) form of a revision hash for display. */
	FString ShortHash(const FString& Hash)
	{
		return Hash.Len() > 12 ? Hash.Left(12) : Hash;
	}

	/** Compose the read-only detail strip text for a selected branch node. */
	FText FormatBranchDetail(const FArcLoreBranchTreeNode& Node)
	{
		const FText CurrentText = Node.bIsCurrent
			? LOCTEXT("BranchIsCurrentYes", "yes")
			: LOCTEXT("BranchIsCurrentNo", "no");

		const FString ParentDisplay = Node.ParentBranchId.IsEmpty()
			? TEXT("(root)")
			: ShortHash(Node.ParentBranchId);

		const FString ForkDisplay = Node.ForkRevision.IsEmpty()
			? TEXT("-")
			: ShortHash(Node.ForkRevision);

		// Local tip: 7-char short hash, or "(none)" when empty.
		const FString LocalShort = Node.TipHash.IsEmpty()
			? TEXT("(none)")
			: Node.TipHash.Left(7);

		// Remote tip: "(no remote)" when bHasRemote=false; "(none)" when empty but remote exists.
		FString RemoteShort;
		if (!Node.bHasRemote)
		{
			RemoteShort = TEXT("(no remote)");
		}
		else if (Node.RemoteTipHash.IsEmpty())
		{
			RemoteShort = TEXT("(none)");
		}
		else
		{
			RemoteShort = Node.RemoteTipHash.Left(7);
		}

		// Sync line.
		FString SyncLine;
		if (Node.bInSync)
		{
			SyncLine = TEXT("In sync");
		}
		else if (Node.bAheadBehindKnown)
		{
			SyncLine = FString::Printf(TEXT("Ahead %d, Behind %d"), Node.Ahead, Node.Behind);
		}
		else
		{
			SyncLine = TEXT("Out of sync");
		}

		return FText::Format(
			LOCTEXT("BranchDetailFormat", "Branch: {0}\n\nTip: {1}\nCurrent: {2}\nForked from: {3} @ {4}\n\nLocal:  {5}\nRemote: {6}\nSync:   {7}"),
			FText::FromString(Node.BranchName),
			FText::FromString(ShortHash(Node.TipHash)),
			CurrentText,
			FText::FromString(ParentDisplay),
			FText::FromString(ForkDisplay),
			FText::FromString(LocalShort),
			FText::FromString(RemoteShort),
			FText::FromString(SyncLine));
	}
}

void SArcLoreBranchGraphPanel::Construct(const FArguments& InArgs)
{
	GraphWidget = SNew(SArcLoreBranchGraph);
	GraphWidget->OnCommitSelected = SArcLoreBranchGraph::FOnCommitSelected::CreateSP(this, &SArcLoreBranchGraphPanel::OnCommitSelected);
	GraphWidget->OnContextMenuOpening = FOnContextMenuOpening::CreateSP(this, &SArcLoreBranchGraphPanel::BuildContextMenu);

	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.ToolTipText(LOCTEXT("RefreshTip", "Re-assemble the cross-branch commit graph."))
				.IsEnabled(this, &SArcLoreBranchGraphPanel::IsIdle)
				.OnClicked(this, &SArcLoreBranchGraphPanel::OnRefreshClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SArcLoreBranchGraphPanel::GetIncludeArchivedState)
				.IsEnabled(this, &SArcLoreBranchGraphPanel::IsIdle)
				.OnCheckStateChanged(this, &SArcLoreBranchGraphPanel::OnIncludeArchivedChanged)
				.ToolTipText(LOCTEXT("IncludeArchivedTip", "Include archived branches in the graph."))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("IncludeArchived", "Include archived"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("NewBranch", "New Branch..."))
				.ToolTipText(LOCTEXT("NewBranchTip", "Create a new branch from the current HEAD."))
				.IsEnabled(this, &SArcLoreBranchGraphPanel::IsIdle)
				.OnClicked(this, &SArcLoreBranchGraphPanel::OnNewBranchClicked)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		// Branch tree (top, primary) + commit drill-down & selected-branch detail (bottom), resizable.
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			// Primary: the horizontal branch tree.
			+ SSplitter::Slot()
			.Value(0.60f)
			[
				SAssignNew(TreeCanvas, SArcLoreBranchTreeCanvas)
			]

			// Drill-down: full commit DAG + read-only branch detail strip.
			+ SSplitter::Slot()
			.Value(0.40f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Vertical)

				+ SSplitter::Slot()
				.Value(0.70f)
				[
					GraphWidget.ToSharedRef()
				]

				+ SSplitter::Slot()
				.Value(0.30f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.0f, 4.0f, 4.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BranchDetailLabel", "Branch details"))
						.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					.Padding(4.0f, 0.0f, 4.0f, 4.0f)
					[
						SAssignNew(DetailBox, SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.AutoWrapText(true)
					]
				]
			]
		]
	];

	// Bind the branch-tree canvas delegates now that it exists (SAssignNew populated TreeCanvas).
	TreeCanvas->OnBranchSelected = SArcLoreBranchTreeCanvas::FOnBranchSelected::CreateSP(this, &SArcLoreBranchGraphPanel::OnTreeBranchSelected);
	TreeCanvas->OnBranchContextMenu = SArcLoreBranchTreeCanvas::FOnBranchContextMenu::CreateSP(this, &SArcLoreBranchGraphPanel::OnTreeBranchContextMenu);

	FArcLoreSourceControlProvider& Provider = GetProvider();
	StateChangedHandle = Provider.RegisterSourceControlStateChanged_Handle(
		FSourceControlStateChanged::FDelegate::CreateSP(this, &SArcLoreBranchGraphPanel::OnSourceControlStateChanged));

	Refresh();
}

SArcLoreBranchGraphPanel::~SArcLoreBranchGraphPanel()
{
	// Guard against provider/module teardown ordering (cf. engine source-control widgets).
	if (StateChangedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("SourceControl"))
	{
		ISourceControlProvider& BaseProvider = ISourceControlModule::Get().GetProvider();
		BaseProvider.UnregisterSourceControlStateChanged_Handle(StateChangedHandle);
		StateChangedHandle.Reset();
	}
}

FArcLoreSourceControlProvider& SArcLoreBranchGraphPanel::GetProvider() const
{
	// Mirror SArcLoreSourceControlPanel: the ArcLore module owns the provider instance.
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	return Module.GetProvider();
}

void SArcLoreBranchGraphPanel::OnSourceControlStateChanged()
{
	// The provider broadcasts on every cached-state change; Refresh() self-guards on IsIdle()
	// so a broadcast fired by our own in-flight op cannot stack a second fetch.
	Refresh();
}

bool SArcLoreBranchGraphPanel::IsIdle() const
{
	return !bOperationInProgress;
}

void SArcLoreBranchGraphPanel::Refresh()
{
	if (!IsIdle())
	{
		return;
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;

	// No-file op; bind the op into the completion delegate so it stays alive and its
	// Branches/Revisions payload is readable on completion.
	TSharedRef<FLoreBranchGraph> Op = ISourceControlOperation::Create<FLoreBranchGraph>();
	Op->bIncludeArchived = bIncludeArchived;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreBranchGraphPanel::OnGraphFetched, Op));
}

void SArcLoreBranchGraphPanel::OnGraphFetched(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchGraph> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	bOperationInProgress = false;

	// Cache the branch list for the tree context-menu lookups (switch/create targets).
	CachedBranches = InOp->Branches;

	// Primary view: the branch tree.
	if (TreeCanvas.IsValid())
	{
		FArcLoreBranchTreeLayout TreeLayout = FArcLoreBranchTreeBuilder::BuildTree(InOp->Branches);
		TreeCanvas->SetLayout(MoveTemp(TreeLayout));
	}

	// Drill-down: build the FULL commit DAG ONCE (do NOT filter Revisions to a single branch —
	// BuildLayout resolves parents across the whole set, so a single-branch subset renders as
	// all-dangling stubs whose parents live on ancestor branches).
	if (GraphWidget.IsValid())
	{
		FArcLoreGraphLayout CommitLayout = FArcLoreGraphLayoutBuilder::BuildLayout(InOp->Branches, InOp->Revisions);
		GraphWidget->SetLayout(MoveTemp(CommitLayout));
	}

	// The freshly rebuilt rows hold new node ptrs; the prior selection is stale. Clear it so the
	// commit context menu does not act on a node no longer in the list, and clear the detail strip
	// until a branch blob is selected.
	SelectedNode.Reset();
	if (DetailBox.IsValid())
	{
		DetailBox->SetText(FText::GetEmpty());
	}
}

void SArcLoreBranchGraphPanel::OnCommitSelected(TSharedPtr<FArcLoreCommitNode> InNode)
{
	using namespace ArcLoreBranchGraphPanel_Internal;

	SelectedNode = InNode;

	if (!DetailBox.IsValid())
	{
		return;
	}

	if (InNode.IsValid())
	{
		DetailBox->SetText(FormatCommitDetail(*InNode));
	}
	else
	{
		DetailBox->SetText(FText::GetEmpty());
	}
}

void SArcLoreBranchGraphPanel::OnTreeBranchSelected(const FArcLoreBranchTreeNode& Node)
{
	using namespace ArcLoreBranchGraphPanel_Internal;

	// Selecting a branch updates only the detail strip; the commit drill-down keeps the full DAG
	// (SArcLoreBranchGraph exposes no scroll/select-by-hash API, so we do not scroll it — Step D).
	if (DetailBox.IsValid())
	{
		DetailBox->SetText(FormatBranchDetail(Node));
	}
}

void SArcLoreBranchGraphPanel::OnTreeBranchContextMenu(const FArcLoreBranchTreeNode& Node)
{
	FMenuBuilder MenuBuilder(true, nullptr);

	// Switch to branch — only offered for a non-current branch. Reuses the exact switch flow
	// (tri-state staged-count dirty guard) used by the commit drill-down's own menu.
	if (!Node.bIsCurrent)
	{
		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("TreeContextSwitch", "Switch to branch '{0}'"), FText::FromString(Node.BranchName)),
			FText::Format(LOCTEXT("TreeContextSwitchTip", "Switch the working copy to branch '{0}'."), FText::FromString(Node.BranchName)),
			FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.Actions.Sync"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SArcLoreBranchGraphPanel::OnSwitchToBranch, Node.BranchName),
				FCanExecuteAction::CreateSP(this, &SArcLoreBranchGraphPanel::IsIdle)));
	}

	// Create child branch — always shown; enabled only when this IS the current branch (the create
	// op forks from HEAD, so a non-current branch must be switched to first).
	const FText CreateTip = Node.bIsCurrent
		? LOCTEXT("TreeContextCreateChildTip", "Create a new branch forked from this branch's HEAD.")
		: LOCTEXT("TreeContextCreateChildDisabledTip", "Switch to this branch first.");

	MenuBuilder.AddMenuEntry(
		LOCTEXT("TreeContextCreateChild", "Create child branch..."),
		CreateTip,
		FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.Actions.Add"),
		FUIAction(
			FExecuteAction::CreateSP(this, &SArcLoreBranchGraphPanel::OnCreateChildBranch),
			FCanExecuteAction::CreateSP(this, &SArcLoreBranchGraphPanel::CanCreateChildOf, Node)));

	// Push the menu at the cursor (mirrors how the commit drill-down summons its row menu).
	FSlateApplication::Get().PushMenu(
		SharedThis(this),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

bool SArcLoreBranchGraphPanel::CanCreateChildOf(FArcLoreBranchTreeNode Node) const
{
	return Node.bIsCurrent && IsIdle();
}

void SArcLoreBranchGraphPanel::OnCreateChildBranch()
{
	// Forks from HEAD — identical to the toolbar "New Branch…" flow.
	PromptAndCreateBranch();
}

TSharedPtr<SWidget> SArcLoreBranchGraphPanel::BuildContextMenu()
{
	// No selection -> no menu (Slate suppresses an empty popup).
	if (!SelectedNode.IsValid())
	{
		return TSharedPtr<SWidget>();
	}

	FMenuBuilder MenuBuilder(true, nullptr);

	// Rewind: offered on ANY selected commit. Gated on IsIdle() (no conflict accessor exists;
	// conflicts surface in the main Lore panel via the working-copy state cache).
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ContextRewind", "Rewind to this revision..."),
		LOCTEXT("ContextRewindTip", "Sync the working copy back to this revision. This mutates files on disk."),
		FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.Actions.Rewind"),
		FUIAction(
			FExecuteAction::CreateSP(this, &SArcLoreBranchGraphPanel::OnRewindToRevision),
			FCanExecuteAction::CreateSP(this, &SArcLoreBranchGraphPanel::IsIdle)));

	// Switch: one entry per branch ref pointing at this commit, excluding the current branch.
	const FString CurrentBranch = GetProvider().GetBranchName();
	if (SelectedNode->BranchRefs.Num() > 0)
	{
		bool bAddedSeparator = false;
		for (int32 RefIndex = 0; RefIndex < SelectedNode->BranchRefs.Num(); ++RefIndex)
		{
			const FString& Ref = SelectedNode->BranchRefs[RefIndex];
			if (Ref.IsEmpty() || Ref == CurrentBranch)
			{
				continue;
			}

			if (!bAddedSeparator)
			{
				MenuBuilder.AddMenuSeparator();
				bAddedSeparator = true;
			}

			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("ContextSwitch", "Switch to branch '{0}'"), FText::FromString(Ref)),
				FText::Format(LOCTEXT("ContextSwitchTip", "Switch the working copy to branch '{0}'."), FText::FromString(Ref)),
				FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.Actions.Sync"),
				FUIAction(
					FExecuteAction::CreateSP(this, &SArcLoreBranchGraphPanel::OnSwitchToBranch, Ref),
					FCanExecuteAction::CreateSP(this, &SArcLoreBranchGraphPanel::IsIdle)));
		}
	}

	return MenuBuilder.MakeWidget();
}

void SArcLoreBranchGraphPanel::OnRewindToRevision()
{
	if (!SelectedNode.IsValid() || !IsIdle())
	{
		return;
	}

	const FString RevisionHash = SelectedNode->Hash;

	// DESTRUCTIVE: a rewind mutates the working copy. Confirm before dispatching.
	const EAppReturnType::Type Choice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::Format(
			LOCTEXT("RewindConfirm", "Sync the working copy to revision {0}? This changes files on disk."),
			FText::FromString(RevisionHash)));
	if (Choice != EAppReturnType::Yes)
	{
		return;
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;

	// The sync worker is bound to engine FSync, which carries the target revision.
	TSharedRef<FSync> Op = ISourceControlOperation::Create<FSync>();
	Op->SetRevision(RevisionHash);
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreBranchGraphPanel::OnSyncComplete));
}

void SArcLoreBranchGraphPanel::OnSyncComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	bOperationInProgress = false;

	if (InResult == ECommandResult::Succeeded)
	{
		UE_LOGF(LogSourceControl, Verbose,"ArcLore: working copy synced (%ls).", *InOperation->GetName().ToString());
	}
	else
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("SyncFailed", "The sync did not complete. See the Output Log for details."));
	}

	// A sync changed the working copy; re-assemble so the tab reflects the new tip/current state.
	Refresh();
}

void SArcLoreBranchGraphPanel::OnSwitchToBranch(FString BranchName)
{
	if (BranchName.IsEmpty() || !IsIdle())
	{
		return;
	}

	// Dirty-state proxy: a staged revision (or, conservatively, any staged file) hard-errors a
	// non-reset switch. The provider exposes only the staged count; gate the tri-state warning on
	// it (no public local-dirty getter / conflict accessor exists — note in the panel header).
	const TOptional<int> StagedCount = GetProvider().GetCachedStagedCount();
	const bool bHasStaged = StagedCount.IsSet() && StagedCount.GetValue() > 0;

	bool bReset = false;

	if (bHasStaged)
	{
		const EAppReturnType::Type Choice = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			FText::Format(
				LOCTEXT("SwitchDirtyWarn",
					"You have staged changes. Switching to '{0}' may be blocked, or with Reset will DISCARD changes to files the target branch modifies.\n\n"
					"Yes = try switch (keep changes), No = switch with RESET (discard), Cancel = abort."),
				FText::FromString(BranchName)));

		if (Choice == EAppReturnType::Yes)
		{
			bReset = false;
		}
		else if (Choice == EAppReturnType::No)
		{
			bReset = true;  // Explicit opt-in: discard local changes.
		}
		else
		{
			// Cancel or dialog closed — abort without dispatching.
			return;
		}
	}

	TSharedRef<FLoreBranchSwitch> Op = ISourceControlOperation::Create<FLoreBranchSwitch>();
	Op->Branch = BranchName;
	Op->bReset = bReset;

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreBranchGraphPanel::OnBranchSwitchComplete, Op));
}

void SArcLoreBranchGraphPanel::OnBranchSwitchComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchSwitch> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	bOperationInProgress = false;

	if (!InOp->bSucceeded)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			FText::Format(
				LOCTEXT("SwitchFailed",
					"Branch switch failed:\n{0}\n\nCommit or unstage your changes first, or use Reset to discard."),
				FText::FromString(InOp->ErrorText)));
	}

	// The working copy / current-branch marker changed; re-assemble the graph.
	Refresh();
}

FReply SArcLoreBranchGraphPanel::OnRefreshClicked()
{
	Refresh();
	return FReply::Handled();
}

ECheckBoxState SArcLoreBranchGraphPanel::GetIncludeArchivedState() const
{
	return bIncludeArchived ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SArcLoreBranchGraphPanel::OnIncludeArchivedChanged(ECheckBoxState InState)
{
	bIncludeArchived = (InState == ECheckBoxState::Checked);
	Refresh();
}

FReply SArcLoreBranchGraphPanel::OnNewBranchClicked()
{
	PromptAndCreateBranch();
	return FReply::Handled();
}

void SArcLoreBranchGraphPanel::PromptAndCreateBranch()
{
	if (!IsIdle())
	{
		return;
	}

	// Modal dialog with a single name field (creation is from current HEAD only; create-at-revision
	// is deferred per plan). Mirrors SArcLoreSourceControlPanel::OnCreateBranchClicked.
	TSharedPtr<SEditableTextBox> NameBox;

	TSharedRef<SWindow> CreateWindow = SNew(SWindow)
		.Title(LOCTEXT("CreateBranchWindowTitle", "Create Branch"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedPtr<SWindow> CreateWindowPtr = CreateWindow;
	TSharedPtr<bool> bConfirmed = MakeShared<bool>(false);

	CreateWindow->SetContent(
		SNew(SBox)
		.MinDesiredWidth(340.0f)
		.Padding(FMargin(16.0f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CreateBranchNameLabel", "Branch name"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SAssignNew(NameBox, SEditableTextBox)
				.HintText(LOCTEXT("CreateBranchNameHint", "Enter branch name..."))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateBranchOK", "OK"))
					.OnClicked_Lambda([CreateWindowPtr, bConfirmed]() -> FReply
					{
						*bConfirmed = true;
						if (CreateWindowPtr.IsValid())
						{
							CreateWindowPtr->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateBranchCancel", "Cancel"))
					.OnClicked_Lambda([CreateWindowPtr]() -> FReply
					{
						if (CreateWindowPtr.IsValid())
						{
							CreateWindowPtr->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
			]
		]
	);

	// Block until the window is dismissed (modal).
	FSlateApplication::Get().AddModalWindow(CreateWindow, FSlateApplication::Get().GetActiveTopLevelWindow());

	if (!(*bConfirmed))
	{
		return;
	}

	const FString BranchName = NameBox.IsValid() ? NameBox->GetText().ToString() : FString();
	if (BranchName.IsEmpty())
	{
		return;
	}

	TSharedRef<FLoreBranchCreate> Op = ISourceControlOperation::Create<FLoreBranchCreate>();
	Op->Name = BranchName;

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreBranchGraphPanel::OnBranchCreateComplete, Op));
}

void SArcLoreBranchGraphPanel::OnBranchCreateComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchCreate> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	bOperationInProgress = false;

	if (!InOp->bSucceeded)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("CreateBranchFailed", "Branch creation failed. See the Output Log for details."));
		return;
	}

	// A new branch exists; re-assemble the graph so its ref appears.
	Refresh();
}

#undef LOCTEXT_NAMESPACE
