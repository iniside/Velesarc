// Copyright Epic Games, Inc. All Rights Reserved.

#include "Panel/SArcLoreBranchHistoryPanel.h"
#include "Panel/ArcLoreAssetDisplay.h"

#include "ArcLoreSourceControlModule.h"
#include "ArcLoreSourceControlProvider.h"
#include "ArcLoreSourceControlState.h"
#include "ArcLoreSourceControlOperations.h"
#include "ArcLoreSourceControlRevision.h"
#include "ISourceControlModule.h"
#include "SourceControlOperations.h"
#include "Modules/ModuleManager.h"
#include "RevisionControlStyle/RevisionControlStyle.h"

#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "SArcLoreBranchHistoryPanel"

// Unique, file-scoped namespace (CLAUDE.md: cpp free functions / helpers in a unique named namespace).
namespace ArcLoreBranchHistory_Internal
{
	// Tree columns. A revision row and a file row reuse the same column set but render
	// distinct widgets per column (the row class switches on node kind).
	const FName ColumnRevision("Revision");
	const FName ColumnDate("Date");
	const FName ColumnAuthor("Author");
	const FName ColumnMessage("Message");

	// Date-range filter option labels. These strings are the stable identity of each
	// option (the combo holds shared pointers to copies of them) AND the matched token
	// in the cutoff computation — keep the two call sites in sync.
	const FString FilterAllTime(TEXT("All time"));
	const FString FilterLast24Hours(TEXT("Last 24 hours"));
	const FString FilterLast7Days(TEXT("Last 7 days"));
	const FString FilterLast30Days(TEXT("Last 30 days"));
	const FString FilterLast90Days(TEXT("Last 90 days"));

	/**
	 * One row of the Branch History tree. A single SMultiColumnTableRow subclass serves
	 * BOTH node kinds: GenerateWidgetForColumn switches on Item->IsRevision()/IsFile() so
	 * revision rows show the expander + rev number + "Local" pill + date/author/message,
	 * while file rows show an action icon + the file path (only in the first two columns).
	 */
	class SArcLoreHistoryRow : public SMultiColumnTableRow<TSharedPtr<FArcLoreHistoryTreeItem>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreHistoryRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLoreHistoryTreeItem>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FArcLoreHistoryTreeItem>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock);
			}

			// Placeholder rows have neither a valid Revision nor a meaningful File; handle
			// them FIRST so the per-kind generators never dereference invalid payloads.
			if (Item->IsPlaceholder())
			{
				return GeneratePlaceholderColumn(InColumnName);
			}

			if (Item->IsRevision())
			{
				return GenerateRevisionColumn(InColumnName);
			}

			return GenerateFileColumn(InColumnName);
		}

	private:
		/** Per-column widget for the synthetic "loading" PLACEHOLDER child. Shows an italic,
		 *  subdued status line in the path column (matching the file node's primary column);
		 *  every other column is empty. */
		TSharedRef<SWidget> GeneratePlaceholderColumn(const FName& InColumnName)
		{
			if (InColumnName == ColumnDate)
			{
				return SNew(STextBlock)
					.Text(LOCTEXT("LoadingChangedFiles", "Loading changed files…"))
					.Font(FAppStyle::Get().GetFontStyle("NormalFontItalic"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground());
			}

			return SNullWidget::NullWidget;
		}

		/** Per-column widget for a REVISION node. */
		TSharedRef<SWidget> GenerateRevisionColumn(const FName& InColumnName)
		{
			const TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> Revision = Item->Revision.ToSharedRef();

			if (InColumnName == ColumnRevision)
			{
				const bool bLocalOnly = Revision->IsLocalOnly();

				// Local-only (committed but not yet pushed) revisions tint the number with
				// an accent color; pushed revisions use the normal foreground.
				const FSlateColor NumberColor = bLocalOnly
					? FSlateColor(FStyleColors::AccentYellow)
					: FSlateColor::UseForeground();

				const TSharedRef<SHorizontalBox> RevBox = SNew(SHorizontalBox)

					// The expander arrow drives lazy child loading via OnExpansionChanged.
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SExpanderArrow, SharedThis(this))
						.IndentAmount(12.0f)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("#%d"), Revision->GetRevisionNumber())))
						.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
						.ColorAndOpacity(NumberColor)
						.ToolTipText(FText::FromString(Revision->RevisionHashHex))
					]

					// Short hash, secondary/subdued, beside the number.
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Revision->GetRevision()))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.ToolTipText(FText::FromString(Revision->RevisionHashHex))
					];

				if (bLocalOnly)
				{
					// Small "Local" badge: an accent-bordered pill calling out that the
					// revision exists only in the local repository (not yet pushed).
					RevBox->AddSlot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
							.BorderBackgroundColor(FStyleColors::AccentYellow)
							.Padding(FMargin(4.0f, 1.0f))
							.ToolTipText(LOCTEXT("LocalOnlyTooltip", "Committed locally — not yet pushed to the remote."))
							[
								SNew(STextBlock)
								.Text(LOCTEXT("LocalBadge", "Local"))
								.Font(FAppStyle::Get().GetFontStyle("SmallFont"))
								.ColorAndOpacity(FStyleColors::Black)
							]
						];
				}

				return RevBox;
			}

			if (InColumnName == ColumnDate)
			{
				return SNew(STextBlock)
					.Text(FText::AsDateTime(Revision->GetDate()))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground());
			}

			if (InColumnName == ColumnAuthor)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Revision->GetUserName()));
			}

			// ColumnMessage (default) — show only the SUMMARY (first line). The full
			// description is shown in the bottom box when the revision is selected.
			const FString FullMessage = Revision->GetDescription();
			FString Summary;
			if (!FullMessage.Split(TEXT("\n"), &Summary, nullptr))
			{
				Summary = FullMessage;
			}
			Summary.TrimStartAndEndInline();
			return SNew(STextBlock)
				.Text(FText::FromString(Summary))
				.ToolTipText(FText::FromString(FullMessage));
		}

		/** Per-column widget for a FILE node (child of a revision). The friendly NAME lives in
		 *  the FIRST column (ColumnRevision, aligned under the revision number) on a single
		 *  clean row — indent + change-action icon + name — the asset TYPE renders under
		 *  ColumnAuthor, and the owning map / folder lives in ColumnMessage. ColumnDate is
		 *  empty so nothing renders "under Date". */
		TSharedRef<SWidget> GenerateFileColumn(const FName& InColumnName)
		{
			// Resolve a human-useful display name via the shared resolver, cached per-node on
			// the file payload so we hit the OFPA label / map derivation at most once per row.
			// Done FIRST because the name column (ColumnRevision) now needs CachedPrimary.
			if (!Item->File.bResolved)
			{
				const FString ResolveFrom = Item->File.AbsPath.IsEmpty() ? Item->File.Path : Item->File.AbsPath;
				const ArcLoreAssetDisplay::FArcLoreDisplayInfo Info = ArcLoreAssetDisplay::ResolveDisplayInfo(ResolveFrom);
				Item->File.CachedPrimary = Info.Primary;
				Item->File.CachedSecondary = Info.Secondary;
				Item->File.CachedTooltip = Info.Tooltip;
				Item->File.bResolved = true;
			}

			if (InColumnName == ColumnRevision)
			{
				// One clean row in the FIRST column: indent + change-action icon + friendly NAME.
				const EArcLoreState::Type Action = Item->File.Action;
				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SExpanderArrow, SharedThis(this))
						.IndentAmount(12.0f)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.WidthOverride(20.0f)
						[
							SNew(SImage)
							.Image_Lambda([Action]() -> const FSlateBrush*
							{
								return FArcLoreSourceControlState::GetIconForState(Action).GetIcon();
							})
						]
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Item->File.CachedPrimary))
						.ToolTipText(FText::FromString(Item->File.CachedTooltip))
					];
			}

			if (InColumnName == ColumnAuthor)
			{
				// Asset type (e.g. "WidgetBlueprint") under the Author column. Resolved once
				// (no-load AssetRegistry), cached per node. The bResolved block's ResolveFrom is
				// out of scope here — recompute the source path (same expression as ColumnMessage).
				if (!Item->File.bTypeResolved)
				{
					const FString TypeFrom = Item->File.AbsPath.IsEmpty() ? Item->File.Path : Item->File.AbsPath;
					Item->File.CachedType = ArcLoreAssetDisplay::ResolveAssetTypeName(TypeFrom);
					Item->File.bTypeResolved = true;
				}

				if (Item->File.CachedType.IsEmpty())
				{
					return SNullWidget::NullWidget;
				}

				return SNew(STextBlock)
					.Text(FText::FromString(Item->File.CachedType))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.ToolTipText(FText::FromString(Item->File.CachedTooltip));
			}

			if (InColumnName == ColumnMessage)
			{
				// Owning map / package folder (subdued) + inline Open/Browse buttons for asset rows.
				const FString AbsPath = Item->File.AbsPath.IsEmpty() ? Item->File.Path : Item->File.AbsPath;
				const ArcLoreAssetDisplay::EArcLoreFileKind Kind = ArcLoreAssetDisplay::ClassifyFile(AbsPath);
				const bool bIsAsset = Kind != ArcLoreAssetDisplay::EArcLoreFileKind::NonAsset;

				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Item->File.CachedSecondary))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.ToolTipText(FText::FromString(Item->File.CachedTooltip))
					];

				if (bIsAsset)
				{
					Row->AddSlot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.ToolTipText(LOCTEXT("InlineOpenTip", "Open this asset"))
							.OnClicked_Lambda([AbsPath]() -> FReply
							{
								ArcLoreAssetDisplay::OpenFile(AbsPath);
								return FReply::Handled();
							})
							[
								SNew(SImage)
								.Image(FAppStyle::GetBrush("Icons.Edit"))
							]
						];

					Row->AddSlot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(2.0f, 0.0f, 0.0f, 0.0f)
						[
							PropertyCustomizationHelpers::MakeBrowseButton(
								FSimpleDelegate::CreateLambda([AbsPath]()
								{
									ArcLoreAssetDisplay::BrowseToFile(AbsPath);
								}),
								LOCTEXT("InlineBrowseTip", "Browse to this asset in the Content Browser"),
								TAttribute<bool>(true))
						];
				}

				return Row;
			}

			// ColumnDate: nothing renders for file rows (name in ColumnRevision, type in ColumnAuthor).
			return SNullWidget::NullWidget;
		}

		TSharedPtr<FArcLoreHistoryTreeItem> Item;
	};

	/** Build a fresh column header row for the revision/file tree. */
	TSharedRef<SHeaderRow> MakeBranchHistoryHeaderRow()
	{
		return SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnRevision)
			.DefaultLabel(LOCTEXT("ColumnRevision", "Revision"))
			.FillWidth(0.30f)

			+ SHeaderRow::Column(ColumnDate)
			.DefaultLabel(LOCTEXT("ColumnDate", "Date"))
			.FillWidth(0.22f)

			+ SHeaderRow::Column(ColumnAuthor)
			.DefaultLabel(LOCTEXT("ColumnAuthor", "Author"))
			.FillWidth(0.18f)

			+ SHeaderRow::Column(ColumnMessage)
			.DefaultLabel(LOCTEXT("ColumnMessage", "Message"))
			.FillWidth(0.30f);
	}
}

void SArcLoreBranchHistoryPanel::Construct(const FArguments& InArgs)
{
	using namespace ArcLoreBranchHistory_Internal;

	// Date-range filter options (newest-first display matches the tree). Default = All time.
	FilterOptions.Add(MakeShared<FString>(FilterAllTime));
	FilterOptions.Add(MakeShared<FString>(FilterLast24Hours));
	FilterOptions.Add(MakeShared<FString>(FilterLast7Days));
	FilterOptions.Add(MakeShared<FString>(FilterLast30Days));
	FilterOptions.Add(MakeShared<FString>(FilterLast90Days));
	SelectedFilter = FilterOptions[0];

	// Sort-key dropdown options (must match LabelForSortKey/SortKeyFromLabel). Default = Name.
	SortKeyOptions.Add(MakeShared<FString>(FString(TEXT("Name"))));
	SortKeyOptions.Add(MakeShared<FString>(FString(TEXT("Path"))));
	SortKeyOptions.Add(MakeShared<FString>(FString(TEXT("Type"))));
	SelectedSortKey = SortKeyOptions[0];

	TreeView = SNew(STreeView<TSharedPtr<FArcLoreHistoryTreeItem>>)
		.TreeItemsSource(&RootItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreBranchHistoryPanel::OnGenerateRow)
		.OnGetChildren(this, &SArcLoreBranchHistoryPanel::OnGetChildren)
		.OnExpansionChanged(this, &SArcLoreBranchHistoryPanel::OnExpansionChanged)
		.OnSelectionChanged(this, &SArcLoreBranchHistoryPanel::OnTreeSelectionChanged)
		.OnContextMenuOpening(this, &SArcLoreBranchHistoryPanel::OnContextMenuOpening)
		.HeaderRow(MakeBranchHistoryHeaderRow());

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
				.ToolTipText(LOCTEXT("RefreshTip", "Re-query the branch revision log."))
				.IsEnabled(this, &SArcLoreBranchHistoryPanel::IsIdle)
				.OnClicked(this, &SArcLoreBranchHistoryPanel::OnRefreshClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SyncToLatest", "Sync to Latest"))
				.ToolTipText(LOCTEXT("SyncToLatestTip", "Sync the working copy to the branch tip."))
				.IsEnabled(this, &SArcLoreBranchHistoryPanel::IsIdle)
				.OnClicked(this, &SArcLoreBranchHistoryPanel::OnSyncToLatestClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CheckInChanges", "Check in Changes..."))
				.ToolTipText(LOCTEXT("CheckInChangesTip", "Open the Lore source-control panel to stage and commit changes."))
				.IsEnabled(this, &SArcLoreBranchHistoryPanel::IsIdle)
				.OnClicked(this, &SArcLoreBranchHistoryPanel::OnCheckInChangesClicked)
			]

			// Date-range filter: "Show:" label + range combo, applied client-side to the
			// already-loaded revisions (no re-query).
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ShowLabel", "Show:"))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(FilterComboBox, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&FilterOptions)
				.InitiallySelectedItem(SelectedFilter)
				.OnGenerateWidget(this, &SArcLoreBranchHistoryPanel::OnGenerateFilterOption)
				.OnSelectionChanged(this, &SArcLoreBranchHistoryPanel::OnFilterSelectionChanged)
				.ToolTipText(LOCTEXT("FilterTip", "Limit the revisions shown to a recent time window."))
				[
					SNew(STextBlock)
					.Text(this, &SArcLoreBranchHistoryPanel::GetSelectedFilterLabel)
				]
			]

			// Sort: dropdown (Name/Path/Type) + asc/desc toggle, applied to the file children
			// within every revision (revision newest-first order is left intact).
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				MakeSortControls()
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]

		// Tree (top) + selected-revision full message (bottom), resizable.
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			+ SSplitter::Slot()
			.Value(0.78f)
			[
				TreeView.ToSharedRef()
			]

			+ SSplitter::Slot()
			.Value(0.22f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 4.0f, 4.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("CommitMessageLabel", "Commit message"))
					.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(4.0f, 0.0f, 4.0f, 4.0f)
				[
					SAssignNew(RevisionMessageBox, SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.AutoWrapText(true)
				]
			]
		]
	];

	FArcLoreSourceControlProvider& Provider = GetProvider();
	StateChangedHandle = Provider.RegisterSourceControlStateChanged_Handle(
		FSourceControlStateChanged::FDelegate::CreateSP(this, &SArcLoreBranchHistoryPanel::OnSourceControlStateChanged));

	RefreshHistory();
}

SArcLoreBranchHistoryPanel::~SArcLoreBranchHistoryPanel()
{
	// Guard against provider/module teardown ordering (cf. engine source-control widgets).
	if (StateChangedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("SourceControl"))
	{
		ISourceControlProvider& BaseProvider = ISourceControlModule::Get().GetProvider();
		BaseProvider.UnregisterSourceControlStateChanged_Handle(StateChangedHandle);
		StateChangedHandle.Reset();
	}
}

FArcLoreSourceControlProvider& SArcLoreBranchHistoryPanel::GetProvider() const
{
	// Mirror SArcLoreSourceControlPanel: the ArcLore module owns the provider instance.
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	return Module.GetProvider();
}

void SArcLoreBranchHistoryPanel::OnSourceControlStateChanged()
{
	// Intentionally do NOT RefreshHistory here. The provider broadcasts this on every cached-state
	// change — including the read-only file-load op that backs lazy revision expansion. Refreshing
	// on that broadcast rebuilds RootItems with fresh nodes, collapsing the just-expanded revision
	// (and discarding its loaded children) a few frames after expand. The history is instead
	// refreshed only on explicit user intent: Construct, the Refresh button, and this panel's own
	// mutations (OnSyncComplete, covering Sync-to-Latest and Rewind).
}

bool SArcLoreBranchHistoryPanel::IsIdle() const
{
	return !bOperationInProgress;
}

void SArcLoreBranchHistoryPanel::RefreshHistory()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	// No-file op; bind the op into the completion delegate so it stays alive and its
	// Revisions payload is readable on completion.
	TSharedRef<FLoreRevisionLog> Op = ISourceControlOperation::Create<FLoreRevisionLog>();
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreBranchHistoryPanel::OnHistoryComplete, Op));
}

void SArcLoreBranchHistoryPanel::OnHistoryComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRevisionLog> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	// The log arrives newest-first (as Lore emits it). Cache the unfiltered revisions; the
	// visible tree is derived from them via the current date-range filter.
	LoadedRevisions = InOp->Revisions;
	RebuildRootsFromFilter();
}

void SArcLoreBranchHistoryPanel::RebuildRootsFromFilter()
{
	// Derive the visible roots from the cached revisions by applying the date-range filter.
	// Each surviving revision gets exactly one placeholder child so STreeView draws the
	// expander arrow (it only does so when OnGetChildren yields a child); the real file
	// children replace the placeholder lazily on first expand (OnRevisionFilesComplete).
	//
	// To survive an explicit Refresh / filter change without collapsing already-loaded revisions,
	// snapshot the OLD top-level nodes (keyed on revision hash) plus which were expanded, then
	// carry their loaded children + expansion onto the freshly built nodes.
	TMap<FString, TSharedPtr<FArcLoreHistoryTreeItem>> PrevByHash;
	TSet<FString> PrevExpandedHashes;
	for (const TSharedPtr<FArcLoreHistoryTreeItem>& OldNode : RootItems)
	{
		if (!OldNode.IsValid() || !OldNode->IsRevision() || !OldNode->Revision.IsValid())
		{
			continue;
		}

		const FString OldHash = OldNode->Revision->RevisionHashHex;
		PrevByHash.Add(OldHash, OldNode);
		if (TreeView.IsValid() && TreeView->IsItemExpanded(OldNode))
		{
			PrevExpandedHashes.Add(OldHash);
		}
	}

	RootItems.Reset();

	FDateTime Cutoff;
	const bool bHasCutoff = TryGetFilterCutoff(Cutoff);

	RootItems.Reserve(LoadedRevisions.Num());
	for (const TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>& Revision : LoadedRevisions)
	{
		if (bHasCutoff && Revision->GetDate() < Cutoff)
		{
			continue;
		}

		TSharedRef<FArcLoreHistoryTreeItem> RevNode = FArcLoreHistoryTreeItem::MakeRevision(Revision);

		const FString NewHash = Revision->RevisionHashHex;
		const TSharedPtr<FArcLoreHistoryTreeItem>* OldNodePtr = PrevByHash.Find(NewHash);
		if (OldNodePtr != nullptr && (*OldNodePtr).IsValid() && (*OldNodePtr)->bChildrenRequested)
		{
			// Carry over the already-loaded real file children so the explicit refresh does not
			// re-query files (and does not flash the "Loading…" placeholder).
			RevNode->Children = (*OldNodePtr)->Children;
			RevNode->bChildrenRequested = true;
		}
		else
		{
			// No loaded children yet — add the placeholder so the expander appears and the first
			// expand still issues the file query (bChildrenRequested stays false).
			RevNode->Children.Add(FArcLoreHistoryTreeItem::MakePlaceholder());
		}

		RootItems.Add(RevNode);
	}

	if (TreeView.IsValid())
	{
		// Re-expand the revisions that were expanded before the rebuild (LoadedRevisions order is
		// preserved, so RootItems is still newest-first).
		for (const TSharedPtr<FArcLoreHistoryTreeItem>& RevNode : RootItems)
		{
			if (RevNode.IsValid() && RevNode->Revision.IsValid()
				&& PrevExpandedHashes.Contains(RevNode->Revision->RevisionHashHex))
			{
				TreeView->SetItemExpansion(RevNode, true);
			}
		}

		TreeView->RequestTreeRefresh();
	}
}

bool SArcLoreBranchHistoryPanel::TryGetFilterCutoff(FDateTime& OutCutoff) const
{
	using namespace ArcLoreBranchHistory_Internal;

	if (!SelectedFilter.IsValid() || *SelectedFilter == FilterAllTime)
	{
		// No lower bound — keep every revision.
		return false;
	}

	// Dates are stored UTC and corrected in FArcLoreSourceControlRevision, so compare against
	// a UTC-now-relative cutoff. (UtcNow is allowed in engine widget code — the Date.now() ban
	// is workflow-script-only.)
	const FDateTime Now = FDateTime::UtcNow();

	if (*SelectedFilter == FilterLast24Hours)
	{
		OutCutoff = Now - FTimespan::FromHours(24.0);
		return true;
	}
	if (*SelectedFilter == FilterLast7Days)
	{
		OutCutoff = Now - FTimespan::FromDays(7.0);
		return true;
	}
	if (*SelectedFilter == FilterLast30Days)
	{
		OutCutoff = Now - FTimespan::FromDays(30.0);
		return true;
	}
	if (*SelectedFilter == FilterLast90Days)
	{
		OutCutoff = Now - FTimespan::FromDays(90.0);
		return true;
	}

	// Unknown label: fail open (keep everything) rather than hide the history.
	return false;
}

TSharedRef<SWidget> SArcLoreBranchHistoryPanel::OnGenerateFilterOption(TSharedPtr<FString> InOption) const
{
	const FString OptionText = InOption.IsValid() ? *InOption : FString();
	return SNew(STextBlock)
		.Text(FText::FromString(OptionText));
}

void SArcLoreBranchHistoryPanel::OnFilterSelectionChanged(TSharedPtr<FString> InNewSelection, ESelectInfo::Type InSelectInfo)
{
	if (!InNewSelection.IsValid())
	{
		return;
	}

	SelectedFilter = InNewSelection;
	RebuildRootsFromFilter();
}

FText SArcLoreBranchHistoryPanel::GetSelectedFilterLabel() const
{
	if (SelectedFilter.IsValid())
	{
		return FText::FromString(*SelectedFilter);
	}
	return FText::GetEmpty();
}

TSharedRef<SWidget> SArcLoreBranchHistoryPanel::MakeSortControls()
{
	return SNew(SHorizontalBox)

		// "Sort:" caption.
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(12.0f, 0.0f, 2.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("SortLabel", "Sort:"))
		]

		// Sort-key dropdown (Name/Path/Type). InitiallySelectedItem seeds the selection WITHOUT
		// firing OnSelectionChanged; the change handler additionally ignores Direct selections so a
		// programmatic SetSelectedItem can never re-trigger a sort (this panel has no header sort).
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			SAssignNew(SortKeyCombo, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&SortKeyOptions)
			.InitiallySelectedItem(SelectedSortKey)
			.OnSelectionChanged(this, &SArcLoreBranchHistoryPanel::OnSortKeySelectionChanged)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
			{
				return SNew(STextBlock)
					.Text(InItem.IsValid() ? FText::FromString(*InItem) : FText::GetEmpty());
			})
			.ToolTipText(LOCTEXT("SortKeyTip", "Sort each revision's changed files by name, path, or asset type."))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::FromString(LabelForSortKey(SortKey));
				})
			]
		]

		// Direction toggle: ascending "^" / descending "v".
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("SortDirTip", "Toggle ascending / descending sort order."))
			.OnClicked(this, &SArcLoreBranchHistoryPanel::OnToggleSortDirectionClicked)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::FromString(SortMode == EColumnSortMode::Descending ? TEXT("v") : TEXT("^"));
				})
			]
		];
}

ArcLoreAssetDisplay::EArcLoreSortKey SArcLoreBranchHistoryPanel::SortKeyFromLabel(const FString& Label) const
{
	if (Label == TEXT("Path"))
	{
		return ArcLoreAssetDisplay::EArcLoreSortKey::Path;
	}
	if (Label == TEXT("Type"))
	{
		return ArcLoreAssetDisplay::EArcLoreSortKey::Type;
	}
	return ArcLoreAssetDisplay::EArcLoreSortKey::Name;
}

FString SArcLoreBranchHistoryPanel::LabelForSortKey(ArcLoreAssetDisplay::EArcLoreSortKey Key) const
{
	switch (Key)
	{
	case ArcLoreAssetDisplay::EArcLoreSortKey::Path:
		return TEXT("Path");
	case ArcLoreAssetDisplay::EArcLoreSortKey::Type:
		return TEXT("Type");
	case ArcLoreAssetDisplay::EArcLoreSortKey::Name:
	default:
		return TEXT("Name");
	}
}

void SArcLoreBranchHistoryPanel::OnSortKeySelectionChanged(TSharedPtr<FString> InNewSelection, ESelectInfo::Type InSelectInfo)
{
	// Ignore the programmatic Direct selection (no spurious re-sort) and empty selections.
	if (!InNewSelection.IsValid() || InSelectInfo == ESelectInfo::Direct)
	{
		return;
	}

	SelectedSortKey = InNewSelection;
	SortKey = SortKeyFromLabel(*InNewSelection);
	SortRevisionFileChildren();
}

FReply SArcLoreBranchHistoryPanel::OnToggleSortDirectionClicked()
{
	SortMode = (SortMode == EColumnSortMode::Descending)
		? EColumnSortMode::Ascending
		: EColumnSortMode::Descending;
	SortRevisionFileChildren();
	return FReply::Handled();
}

void SArcLoreBranchHistoryPanel::SortFileChildrenOf(FArcLoreHistoryTreeItem& RevisionNode)
{
	// Only sort REAL file children: an unexpanded (or just-collapsed) revision carries a lone
	// synthetic "loading" placeholder, which IsFile() reports false for — sorting/Type-resolving
	// it would resolve an empty path.
	if (RevisionNode.Children.Num() == 0
		|| !RevisionNode.Children[0].IsValid()
		|| !RevisionNode.Children[0]->IsFile())
	{
		return;
	}

	// Named lambdas (must outlive the TFunctionRef params bound from them). Each reads the node's
	// File payload, mirroring SArcLoreSourceControlPanel::SortRevisionFiles.
	const TFunction<FString(const FArcLoreHistoryTreeItem&)> GetAbs =
		[](const FArcLoreHistoryTreeItem& Node) -> FString
		{
			return Node.File.AbsPath.IsEmpty() ? Node.File.Path : Node.File.AbsPath;
		};
	const TFunction<FString& (FArcLoreHistoryTreeItem&)> GetTypeRef =
		[](FArcLoreHistoryTreeItem& Node) -> FString& { return Node.File.CachedType; };
	const TFunction<bool& (FArcLoreHistoryTreeItem&)> GetTypeResolvedRef =
		[](FArcLoreHistoryTreeItem& Node) -> bool& { return Node.File.bTypeResolved; };

	ArcLoreAssetDisplay::SortFileItems<FArcLoreHistoryTreeItem>(
		RevisionNode.Children, SortKey, SortMode, GetAbs, GetTypeRef, GetTypeResolvedRef);
}

void SArcLoreBranchHistoryPanel::SortRevisionFileChildren()
{
	// Sort the file children of EVERY revision so the order is consistent whether a revision was
	// expanded before or after the user changed the sort. Revision (newest-first) order is untouched.
	for (const TSharedPtr<FArcLoreHistoryTreeItem>& RevNode : RootItems)
	{
		if (RevNode.IsValid())
		{
			SortFileChildrenOf(*RevNode);
		}
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

void SArcLoreBranchHistoryPanel::OnGetChildren(TSharedPtr<FArcLoreHistoryTreeItem> InItem, TArray<TSharedPtr<FArcLoreHistoryTreeItem>>& OutChildren)
{
	if (InItem.IsValid())
	{
		OutChildren = InItem->Children;
	}
}

TSharedRef<ITableRow> SArcLoreBranchHistoryPanel::OnGenerateRow(TSharedPtr<FArcLoreHistoryTreeItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLoreBranchHistory_Internal;
	return SNew(SArcLoreHistoryRow, OwnerTable).Item(InItem);
}

void SArcLoreBranchHistoryPanel::OnExpansionChanged(TSharedPtr<FArcLoreHistoryTreeItem> InItem, bool bExpanded)
{
	// Only revision rows have lazy children; ignore file rows and collapses.
	if (!bExpanded || !InItem.IsValid() || !InItem->IsRevision())
	{
		return;
	}

	// Issue the changed-file query at most once per revision row. Mark the request BEFORE
	// dispatching so a rapid collapse/expand toggle cannot fire a second op.
	if (InItem->bChildrenRequested)
	{
		return;
	}
	InItem->bChildrenRequested = true;

	FArcLoreSourceControlProvider& Provider = GetProvider();
	TSharedRef<FLoreRevisionFiles> Op = ISourceControlOperation::Create<FLoreRevisionFiles>();
	Op->Revision = InItem->Revision->RevisionHashHex;

	// Capture the target node weakly: the tree may be rebuilt (RefreshHistory) before this
	// op completes, in which case the node is stale and the result is dropped.
	TWeakPtr<FArcLoreHistoryTreeItem> WeakItem = InItem;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreBranchHistoryPanel::OnRevisionFilesComplete, Op, WeakItem));
}

void SArcLoreBranchHistoryPanel::OnTreeSelectionChanged(TSharedPtr<FArcLoreHistoryTreeItem> InItem, ESelectInfo::Type InSelectInfo)
{
	if (!RevisionMessageBox.IsValid())
	{
		return;
	}

	// Show the full commit message for a selected revision row; clear it for file rows,
	// placeholders, or an empty selection.
	if (InItem.IsValid() && InItem->IsRevision() && InItem->Revision.IsValid())
	{
		RevisionMessageBox->SetText(FText::FromString(InItem->Revision->GetDescription()));
	}
	else
	{
		RevisionMessageBox->SetText(FText::GetEmpty());
	}
}

void SArcLoreBranchHistoryPanel::OnRevisionFilesComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRevisionFiles> InOp, TWeakPtr<FArcLoreHistoryTreeItem> TargetItem)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	TSharedPtr<FArcLoreHistoryTreeItem> Item = TargetItem.Pin();
	if (!Item.IsValid())
	{
		// The owning revision node was replaced by a refresh; nothing to populate.
		return;
	}

	// Fill each file's absolute on-disk path BEFORE building its node, so GenerateFileColumn's
	// shared resolver can derive the friendly name / owning map (mirrors the main panel's
	// outgoing fill). Lore emits repo-relative, forward-slash paths.
	const FString RepoRoot = GetProvider().GetPathToRepositoryRoot();

	Item->Children.Reset();
	Item->Children.Reserve(InOp->Files.Num());
	for (const FArcLoreUnpushedFile& File : InOp->Files)
	{
		FArcLoreUnpushedFile FileCopy = File;
		FileCopy.AbsPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(RepoRoot, FileCopy.Path));
		Item->Children.Add(FArcLoreHistoryTreeItem::MakeFile(FileCopy));
	}

	// Apply the active sort to the freshly built children so a revision expanded after the user
	// chose a sort arrives already ordered.
	SortFileChildrenOf(*Item);

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}
}

TSharedRef<SHeaderRow> SArcLoreBranchHistoryPanel::MakeHeaderRow() const
{
	return ArcLoreBranchHistory_Internal::MakeBranchHistoryHeaderRow();
}

TSharedPtr<SWidget> SArcLoreBranchHistoryPanel::OnContextMenuOpening()
{
	if (!TreeView.IsValid())
	{
		return TSharedPtr<SWidget>();
	}

	TArray<TSharedPtr<FArcLoreHistoryTreeItem>> Selected = TreeView->GetSelectedItems();
	if (Selected.Num() == 0 || !Selected[0].IsValid())
	{
		return TSharedPtr<SWidget>();
	}

	const TSharedPtr<FArcLoreHistoryTreeItem> Item = Selected[0];

	// The synthetic "loading" placeholder has no Revision/File payload — offer no menu.
	if (Item->IsPlaceholder())
	{
		return TSharedPtr<SWidget>();
	}

	FMenuBuilder MenuBuilder(true, nullptr);

	if (Item->IsRevision())
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ContextRewind", "Sync to this revision (Rewind)..."),
			LOCTEXT("ContextRewindTip", "Sync the working copy back to this revision. This mutates the working copy."),
			FSlateIcon(FRevisionControlStyleManager::Get().GetStyleSetName(), "RevisionControl.Actions.Rewind"),
			FUIAction(
				FExecuteAction::CreateSP(this, &SArcLoreBranchHistoryPanel::OnRewindToRevision, Item),
				FCanExecuteAction::CreateSP(this, &SArcLoreBranchHistoryPanel::IsIdle)));
	}
	else if (Item->IsFile())
	{
		// Kind-correct Open / Browse via the shared util (plain asset vs OFPA-map-backed).
		const FString AbsPath = Item->File.AbsPath.IsEmpty() ? Item->File.Path : Item->File.AbsPath;
		const ArcLoreAssetDisplay::EArcLoreFileKind Kind = ArcLoreAssetDisplay::ClassifyFile(AbsPath);

		if (!AbsPath.IsEmpty() && Kind != ArcLoreAssetDisplay::EArcLoreFileKind::NonAsset)
		{
			const bool bIsMapBacked = Kind == ArcLoreAssetDisplay::EArcLoreFileKind::ExternalActor
				|| Kind == ArcLoreAssetDisplay::EArcLoreFileKind::ExternalObject;

			const FText OpenLabel = bIsMapBacked
				? LOCTEXT("ContextOpenMap", "Open Map")
				: LOCTEXT("ContextOpenAsset", "Open Asset");
			const FText OpenTip = bIsMapBacked
				? LOCTEXT("ContextOpenMapTip", "Open the owning map of this external actor/object as a level.")
				: LOCTEXT("ContextOpenAssetTip", "Open this asset in its asset editor.");
			const FText BrowseLabel = bIsMapBacked
				? LOCTEXT("ContextBrowseMap", "Browse to Map")
				: LOCTEXT("ContextBrowseAsset", "Browse to Asset");
			const FText BrowseTip = bIsMapBacked
				? LOCTEXT("ContextBrowseMapTip", "Reveal the owning map in the Content Browser.")
				: LOCTEXT("ContextBrowseAssetTip", "Reveal this asset in the Content Browser.");

			MenuBuilder.AddMenuEntry(
				OpenLabel,
				OpenTip,
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([AbsPath]() { ArcLoreAssetDisplay::OpenFile(AbsPath); })));

			MenuBuilder.AddMenuEntry(
				BrowseLabel,
				BrowseTip,
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([AbsPath]() { ArcLoreAssetDisplay::BrowseToFile(AbsPath); })));
		}
	}

	return MenuBuilder.MakeWidget();
}

void SArcLoreBranchHistoryPanel::OnRewindToRevision(TSharedPtr<FArcLoreHistoryTreeItem> InItem)
{
	if (!InItem.IsValid() || !InItem->IsRevision() || !IsIdle())
	{
		return;
	}

	const FString RevisionHash = InItem->Revision->RevisionHashHex;

	// DESTRUCTIVE: a rewind mutates the working copy. Confirm before dispatching.
	const EAppReturnType::Type Choice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::Format(
			LOCTEXT("RewindConfirm", "Sync the working copy to revision #{0}? This changes files on disk."),
			FText::AsNumber(InItem->Revision->GetRevisionNumber())));
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
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreBranchHistoryPanel::OnSyncComplete));
}

FReply SArcLoreBranchHistoryPanel::OnRefreshClicked()
{
	RefreshHistory();
	return FReply::Handled();
}

FReply SArcLoreBranchHistoryPanel::OnSyncToLatestClicked()
{
	if (!IsIdle())
	{
		return FReply::Handled();
	}

	// Confirm: a sync mutates the working copy.
	const EAppReturnType::Type Choice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("SyncLatestConfirm", "Sync the working copy to the branch tip? This changes files on disk."));
	if (Choice != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;

	// FSync with no SetRevision => sync to tip (preserves the existing default behavior).
	TSharedRef<FSync> Op = ISourceControlOperation::Create<FSync>();
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreBranchHistoryPanel::OnSyncComplete));

	return FReply::Handled();
}

FReply SArcLoreBranchHistoryPanel::OnCheckInChangesClicked()
{
	// Reuse the existing Lore source-control panel's commit UX (no new check-in logic).
	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(FArcLoreSourceControlModule::ArcLoreSourceControlTabName));
	return FReply::Handled();
}

void SArcLoreBranchHistoryPanel::OnSyncComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	bOperationInProgress = false;

	// Surface the outcome. Conflicts (if any) are plumbed through the provider's working-copy
	// state and surface in the main Lore panel — do not fabricate a conflict UI here.
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

	// A sync changed the working copy; the revision log itself is unchanged, but refresh so
	// the tab reflects any new tip/local-only state.
	RefreshHistory();
}

#undef LOCTEXT_NAMESPACE
