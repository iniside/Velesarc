// Copyright Epic Games, Inc. All Rights Reserved.

#include "Panel/SArcLoreSourceControlPanel.h"

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
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/Paths.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Misc/PackagePath.h"
#if WITH_EDITOR
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "WorldPartition/WorldPartitionActorDescUtils.h"
#endif
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE "SArcLoreSourceControlPanel"

// Unique, file-scoped namespace (CLAUDE.md: cpp free functions / helpers in a unique named namespace).
namespace ArcLorePanel_Internal
{
	const FName ColumnIcon("Icon");
	const FName ColumnName("Name");
	const FName ColumnType("Type");
	const FName ColumnLock("Lock");
	const FName ColumnPath("Path");

	// Past this many Uncontrolled files the panel shows a count summary instead of the list
	// (rendering thousands of rows is slow and unhelpful).
	constexpr int32 kUncontrolledDisplayLimit = 500;

	// Outgoing (committed-but-unpushed) columns.
	const FName ColumnOutgoingIcon("OutgoingIcon");
	const FName ColumnOutgoingName("OutgoingName");
	const FName ColumnOutgoingType("OutgoingType");
	const FName ColumnOutgoingPath("OutgoingPath");

	// History (commit-log) columns.
	const FName ColumnRev("Rev");
	const FName ColumnAuthor("Author");
	const FName ColumnDate("Date");
	const FName ColumnMessage("Message");

	// Repositories: branch columns.
	const FName ColumnBranch("Branch");
	const FName ColumnLocation("Location");
	const FName ColumnCreated("Created");
	const FName ColumnCreator("Creator");

	// Repositories: instance columns.
	const FName ColumnInstanceBranch("InstanceBranch");
	const FName ColumnStale("Stale");

	// Repositories: remote-repo column.
	const FName ColumnRepoName("RepoName");

	// Locks (branch-wide lock browser) columns.
	const FName ColumnLockPath("LockPath");
	const FName ColumnLockOwner("LockOwner");
	const FName ColumnLockAcquired("LockAcquired");

	/** True when a state belongs in the Staged section (in Lore's staged changeset, any action). */
	bool IsStaged(const FSourceControlStateRef& InState)
	{
		const FArcLoreSourceControlState& Arc = static_cast<const FArcLoreSourceControlState&>(InState.Get());
		return Arc.bStaged;
	}

	/** True when a state belongs in the Dirty (unstaged-changes) section. */
	bool IsDirtyUnstaged(const FSourceControlStateRef& InState)
	{
		// Anything with local changes that is NOT staged: modified, untracked, deleted.
		// NotControlled (an untracked file) bins here too so the user can select it and
		// click Stage (which runs MarkForAdd). Its "?" icon comes from GetIcon().
		const FArcLoreSourceControlState& Arc = static_cast<const FArcLoreSourceControlState&>(InState.Get());
		// NOTE: !IsCurrent() (bIsNewerOnRemote / is_remote_ahead) is intentionally NOT a
		// Dirty criterion. is_remote_ahead is a BRANCH-divergence flag ("the remote has
		// commits we lack"), not a local working-copy change — treating it as Dirty made
		// any per-file-statused asset (e.g. right-clicked) appear modified. The flag still
		// drives the Content Browser "out of date" overlay via IsCurrent(); it just doesn't
		// belong in the local changes list.
		return !Arc.bStaged && (InState->IsModified() || InState->IsDeleted()
			|| Arc.WorkingCopyState == EArcLoreState::NotControlled);
	}

	/** Abs-path accessor for the changes-section item struct. */
	FString FileItemAbs(const FArcLorePanelFileItem& Item)
	{
		return Item.State->GetFilename();
	}

	/** Abs-path accessor for the unpushed/revision file struct (AbsPath, Path fallback). */
	FString UnpushedAbs(const FArcLoreUnpushedFile& File)
	{
		return File.AbsPath.IsEmpty() ? File.Path : File.AbsPath;
	}

	/** Row widget for a single file item (Icon / Name / Path columns). */
	class SArcLoreFileRow : public SMultiColumnTableRow<TSharedPtr<FArcLorePanelFileItem>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreFileRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLorePanelFileItem>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FArcLorePanelFileItem>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			const FSourceControlStateRef State = Item->State;

			// Resolve a useful display name once per item, lazily (the asset-registry /
			// world-partition lookups are not free) and cache it on the item.
			if (!Item->bResolved)
			{
				const ArcLoreAssetDisplay::FArcLoreDisplayInfo Info = ArcLoreAssetDisplay::ResolveDisplayInfo(State->GetFilename());
				Item->CachedPrimary = Info.Primary;
				Item->CachedSecondary = Info.Secondary;
				Item->CachedTooltip = Info.Tooltip;
				Item->bResolved = true;
			}

			// Resolve the asset Type (class short name) once per item, no-load.
			if (!Item->bTypeResolved)
			{
				Item->CachedType = ArcLoreAssetDisplay::ResolveAssetTypeName(State->GetFilename());
				Item->bTypeResolved = true;
			}

			if (InColumnName == ColumnIcon)
			{
				return SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(20.0f)
					[
						SNew(SImage)
						.Image_Lambda([State]() -> const FSlateBrush*
						{
							return State->GetIcon().GetIcon();
						})
						.ToolTipText_Lambda([State]() -> FText
						{
							return State->GetDisplayTooltip();
						})
					];
			}

			if (InColumnName == ColumnName)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->CachedPrimary))
					.ToolTipText(FText::FromString(Item->CachedTooltip));
			}

			if (InColumnName == ColumnLock)
			{
				const FArcLoreSourceControlState& ArcState = static_cast<const FArcLoreSourceControlState&>(State.Get());
				if (!ArcState.IsLocked())
				{
					return SNullWidget::NullWidget;
				}

				const FString DisplayOwner = ArcState.bLockedByMe
					? TEXT("me")
					: ArcState.LockOwner;
				const FText LockLabel = FText::Format(
					LOCTEXT("LockBadgeLabel", "Locked by {0}"),
					FText::FromString(DisplayOwner));
				const FText LockTooltip = FText::FromString(ArcState.LockAcquiredAt.ToString());

				return SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 3.0f, 0.0f)
					[
						SNew(SImage)
						.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Locked"))
						.ToolTipText(LockTooltip)
					]
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LockLabel)
						.ToolTipText(LockTooltip)
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					];
			}

			if (InColumnName == ColumnType)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->CachedType))
					.ToolTipText(FText::FromString(Item->CachedTooltip))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground());
			}

			// ColumnPath (default) — secondary text + inline Open/Browse buttons for asset rows.
			{
				const FString AbsPath = Item->State->GetFilename();
				const ArcLoreAssetDisplay::EArcLoreFileKind Kind = ArcLoreAssetDisplay::ClassifyFile(AbsPath);
				const bool bIsAsset = Kind != ArcLoreAssetDisplay::EArcLoreFileKind::NonAsset;

				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Item->CachedSecondary))
						.ToolTipText(FText::FromString(Item->CachedTooltip))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
		}

	private:
		TSharedPtr<FArcLorePanelFileItem> Item;
	};

	/** Row widget for a single outgoing (committed-but-unpushed) file entry — the
	 *  History sub-list under the revision log still uses a flat FArcLoreUnpushedFile
	 *  list, so this row is retained for OnGenerateRevisionFileRow. */
	class SArcLoreOutgoingRow : public SMultiColumnTableRow<TSharedPtr<FArcLoreUnpushedFile>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreOutgoingRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLoreUnpushedFile>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FArcLoreUnpushedFile>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock);
			}

			// Resolve a useful display name once per item, lazily. Outgoing/revision items
			// carry an absolute path (AbsPath) filled game-side; if absent, fall back to the
			// repo-relative Path so the row still degrades to a clean filename.
			if (!Item->bResolved)
			{
				const FString ResolveFrom = Item->AbsPath.IsEmpty() ? Item->Path : Item->AbsPath;
				const ArcLoreAssetDisplay::FArcLoreDisplayInfo Info = ArcLoreAssetDisplay::ResolveDisplayInfo(ResolveFrom);
				Item->CachedPrimary = Info.Primary;
				Item->CachedSecondary = Info.Secondary;
				Item->CachedTooltip = Info.Tooltip;
				Item->bResolved = true;
			}

			// Resolve the asset Type (class short name) once per item, no-load.
			if (!Item->bTypeResolved)
			{
				const FString TypeFrom = Item->AbsPath.IsEmpty() ? Item->Path : Item->AbsPath;
				Item->CachedType = ArcLoreAssetDisplay::ResolveAssetTypeName(TypeFrom);
				Item->bTypeResolved = true;
			}

			if (InColumnName == ColumnOutgoingIcon)
			{
				const EArcLoreState::Type Action = Item->Action;
				return SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(20.0f)
					[
						SNew(SImage)
						.Image_Lambda([Action]() -> const FSlateBrush*
						{
							return FArcLoreSourceControlState::GetIconForState(Action).GetIcon();
						})
					];
			}

			if (InColumnName == ColumnOutgoingName)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->CachedPrimary))
					.ToolTipText(FText::FromString(Item->CachedTooltip));
			}

			if (InColumnName == ColumnOutgoingType)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->CachedType))
					.ToolTipText(FText::FromString(Item->CachedTooltip))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground());
			}

			// ColumnOutgoingPath (default)
			return SNew(STextBlock)
				.Text(FText::FromString(Item->CachedSecondary))
				.ToolTipText(FText::FromString(Item->CachedTooltip))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground());
		}

	private:
		TSharedPtr<FArcLoreUnpushedFile> Item;
	};

	/** Row widget for one node in the Outgoing TREE (grouped by commit). A commit node
	 *  shows an expander + bold "#<Number>  <first line of message>" in the Name column
	 *  and the commit date in the Path column; a file node reuses the friendly-name
	 *  resolution (asset/actor label + folder, full path tooltip) under its commit. */
	class SArcLoreOutgoingTreeRow : public SMultiColumnTableRow<TSharedPtr<FArcLoreOutgoingNode>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreOutgoingTreeRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLoreOutgoingNode>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FArcLoreOutgoingNode>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock);
			}

			if (Item->IsCommit())
			{
				return GenerateCommitColumn(InColumnName);
			}

			return GenerateFileColumn(InColumnName);
		}

	private:
		/** Per-column widget for a COMMIT node. */
		TSharedRef<SWidget> GenerateCommitColumn(const FName& InColumnName)
		{
			const TSharedPtr<FArcLoreUnpushedCommit> Commit = Item->Commit;

			if (InColumnName == ColumnOutgoingName)
			{
				// First line of the message only (keep the header to a single row).
				FString FirstLine = Commit->Message;
				int32 NewlineIndex = INDEX_NONE;
				if (FirstLine.FindChar(TEXT('\n'), NewlineIndex))
				{
					FirstLine.LeftInline(NewlineIndex);
				}
				FirstLine.TrimEndInline();

				return SNew(SHorizontalBox)

					// Expander arrow drives expand/collapse of the file children.
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SExpanderArrow, SharedThis(this))
						.IndentAmount(12.0f)
					]

					// Bold "#<Number>".
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("#%d"), Commit->RevisionNumber)))
						.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
						.ToolTipText(FText::FromString(Commit->RevisionHashHex))
					]

					// First line of the commit message, beside the number.
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					.Padding(8.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FirstLine))
						.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
						.ToolTipText(FText::FromString(Commit->Message))
					];
			}

			// Commit nodes have no asset Type — leave the Type cell empty.
			if (InColumnName == ColumnOutgoingType)
			{
				return SNullWidget::NullWidget;
			}

			// ColumnOutgoingPath (default) — the commit date.
			return SNew(STextBlock)
				.Text(FText::AsDateTime(Commit->Date))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground());
		}

		/** Per-column widget for a FILE node (child of a commit). */
		TSharedRef<SWidget> GenerateFileColumn(const FName& InColumnName)
		{
			const TSharedPtr<FArcLoreUnpushedFile> File = Item->File;

			// Resolve a useful display name once per item, lazily (cached on the file
			// payload). AbsPath is filled game-side; if absent, fall back to the
			// repo-relative Path so the row still degrades to a clean filename.
			if (!File->bResolved)
			{
				const FString ResolveFrom = File->AbsPath.IsEmpty() ? File->Path : File->AbsPath;
				const ArcLoreAssetDisplay::FArcLoreDisplayInfo Info = ArcLoreAssetDisplay::ResolveDisplayInfo(ResolveFrom);
				File->CachedPrimary = Info.Primary;
				File->CachedSecondary = Info.Secondary;
				File->CachedTooltip = Info.Tooltip;
				File->bResolved = true;
			}

			// Resolve the asset Type (class short name) once per item, no-load.
			if (!File->bTypeResolved)
			{
				const FString TypeFrom = File->AbsPath.IsEmpty() ? File->Path : File->AbsPath;
				File->CachedType = ArcLoreAssetDisplay::ResolveAssetTypeName(TypeFrom);
				File->bTypeResolved = true;
			}

			if (InColumnName == ColumnOutgoingName)
			{
				const EArcLoreState::Type Action = File->Action;
				return SNew(SHorizontalBox)

					// Indent file rows under their commit (an expander slot for alignment).
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SExpanderArrow, SharedThis(this))
						.IndentAmount(12.0f)
					]

					// Change-action icon.
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

					// Friendly name.
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(File->CachedPrimary))
						.ToolTipText(FText::FromString(File->CachedTooltip))
					];
			}

			if (InColumnName == ColumnOutgoingType)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(File->CachedType))
					.ToolTipText(FText::FromString(File->CachedTooltip))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground());
			}

			// ColumnOutgoingPath (default) — owning map / package folder + inline Open/Browse buttons.
			{
				const FString AbsPath = File->AbsPath.IsEmpty() ? File->Path : File->AbsPath;
				const ArcLoreAssetDisplay::EArcLoreFileKind Kind = ArcLoreAssetDisplay::ClassifyFile(AbsPath);
				const bool bIsAsset = Kind != ArcLoreAssetDisplay::EArcLoreFileKind::NonAsset;

				TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(File->CachedSecondary))
						.ToolTipText(FText::FromString(File->CachedTooltip))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
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
		}

		TSharedPtr<FArcLoreOutgoingNode> Item;
	};

	/** Row widget for a single branch revision (Rev / Author / Date / Message columns). */
	class SArcLoreCommitRow : public SMultiColumnTableRow<TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreCommitRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLoreSourceControlRevision>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock);
			}

			if (InColumnName == ColumnRev)
			{
				const bool bLocalOnly = Item->IsLocalOnly();

				// The revision number leads the cell, prominent and bold. Local-only
				// (committed but not yet pushed) revisions tint the number with an
				// accent color; pushed revisions use the normal foreground.
				const FSlateColor NumberColor = bLocalOnly
					? FSlateColor(FStyleColors::AccentYellow)
					: FSlateColor::UseForeground();

				const TSharedRef<SHorizontalBox> RevBox = SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(FString::Printf(TEXT("#%d"), Item->GetRevisionNumber())))
						.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
						.ColorAndOpacity(NumberColor)
						.ToolTipText(FText::FromString(Item->RevisionHashHex))
					]

					// Short hash, secondary/subdued, beside the number.
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(6.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(Item->GetRevision()))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.ToolTipText(FText::FromString(Item->RevisionHashHex))
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

			if (InColumnName == ColumnAuthor)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->GetUserName()));
			}

			if (InColumnName == ColumnDate)
			{
				const FDateTime& Date = Item->GetDate();
				return SNew(STextBlock)
					.Text(FText::FromString(Date.ToString()))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground());
			}

			// ColumnMessage (default)
			return SNew(STextBlock)
				.Text(FText::FromString(Item->GetDescription()))
				.ToolTipText(FText::FromString(Item->GetDescription()));
		}

	private:
		TSharedPtr<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> Item;
	};

	/** Build a fresh column header row for the History (commit-log) list. */
	TSharedRef<SHeaderRow> MakeCommitHeaderRow()
	{
		return SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnRev)
			.DefaultLabel(LOCTEXT("ColumnRev", "Revision"))
			.FillWidth(0.22f)

			+ SHeaderRow::Column(ColumnAuthor)
			.DefaultLabel(LOCTEXT("ColumnAuthor", "Author"))
			.FillWidth(0.2f)

			+ SHeaderRow::Column(ColumnDate)
			.DefaultLabel(LOCTEXT("ColumnDate", "Date"))
			.FillWidth(0.25f)

			+ SHeaderRow::Column(ColumnMessage)
			.DefaultLabel(LOCTEXT("ColumnMessage", "Message"))
			.FillWidth(0.33f);
	}

	/** Row widget for a single branch (Branch / Location / Created / Creator columns). */
	class SArcLoreBranchRow : public SMultiColumnTableRow<TSharedPtr<FArcLoreBranchEntry>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreBranchRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLoreBranchEntry>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FArcLoreBranchEntry>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock);
			}

			if (InColumnName == ColumnBranch)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->Name))
					.Font(Item->bIsCurrent
						? FAppStyle::GetFontStyle("BoldFont")
						: FAppStyle::GetFontStyle("NormalFont"));
			}

			if (InColumnName == ColumnLocation)
			{
				FString Location = Item->bRemote ? TEXT("remote") : TEXT("local");
				if (Item->bArchived)
				{
					Location += TEXT(" (archived)");
				}
				return SNew(STextBlock)
					.Text(FText::FromString(Location))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground());
			}

			if (InColumnName == ColumnCreated)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->Created.ToString()))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground());
			}

			// ColumnCreator (default)
			return SNew(STextBlock)
				.Text(FText::FromString(Item->Creator));
		}

	private:
		TSharedPtr<FArcLoreBranchEntry> Item;
	};

	/** Row widget for a single checkout instance (Path / Branch / Stale columns). */
	class SArcLoreInstanceRow : public SMultiColumnTableRow<TSharedPtr<FArcLoreInstanceEntry>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreInstanceRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLoreInstanceEntry>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FArcLoreInstanceEntry>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock);
			}

			if (InColumnName == ColumnPath)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->Path))
					.ToolTipText(FText::FromString(Item->Path));
			}

			if (InColumnName == ColumnInstanceBranch)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->BranchName));
			}

			// ColumnStale (default)
			return SNew(STextBlock)
				.Text(FText::FromString(Item->bStale ? TEXT("stale") : TEXT("")))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground());
		}

	private:
		TSharedPtr<FArcLoreInstanceEntry> Item;
	};

	/** Row widget for a single remote repository (single Name column). */
	class SArcLoreRepoRow : public SMultiColumnTableRow<TSharedPtr<FArcLoreRepoEntry>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreRepoRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLoreRepoEntry>, Item)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			SMultiColumnTableRow<TSharedPtr<FArcLoreRepoEntry>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock);
			}

			// ColumnRepoName (only column)
			return SNew(STextBlock)
				.Text(FText::FromString(Item->Name));
		}

	private:
		TSharedPtr<FArcLoreRepoEntry> Item;
	};

	/** Build a fresh column header row for the Branches list. */
	TSharedRef<SHeaderRow> MakeBranchHeaderRow()
	{
		return SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnBranch)
			.DefaultLabel(LOCTEXT("ColumnBranch", "Branch"))
			.FillWidth(0.35f)

			+ SHeaderRow::Column(ColumnLocation)
			.DefaultLabel(LOCTEXT("ColumnLocation", "Location"))
			.FillWidth(0.2f)

			+ SHeaderRow::Column(ColumnCreated)
			.DefaultLabel(LOCTEXT("ColumnCreated", "Created"))
			.FillWidth(0.25f)

			+ SHeaderRow::Column(ColumnCreator)
			.DefaultLabel(LOCTEXT("ColumnCreator", "Creator"))
			.FillWidth(0.2f);
	}

	/** Build a fresh column header row for the Instances list. */
	TSharedRef<SHeaderRow> MakeInstanceHeaderRow()
	{
		return SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnPath)
			.DefaultLabel(LOCTEXT("ColumnPath", "Path"))
			.FillWidth(0.6f)

			+ SHeaderRow::Column(ColumnInstanceBranch)
			.DefaultLabel(LOCTEXT("ColumnBranch", "Branch"))
			.FillWidth(0.25f)

			+ SHeaderRow::Column(ColumnStale)
			.DefaultLabel(LOCTEXT("ColumnStale", "Stale"))
			.FillWidth(0.15f);
	}

	/** Build a fresh column header row for the Remote-repos list. */
	TSharedRef<SHeaderRow> MakeRepoHeaderRow()
	{
		return SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnRepoName)
			.DefaultLabel(LOCTEXT("ColumnName", "Name"))
			.FillWidth(1.0f);
	}

	/** Row widget for a single branch lock (Path / Owner / Acquired columns). The
	 *  owner is shown as "me" when its RAW Lore user id matches the provider's resolved
	 *  user id (passed in via MyUserId), else the raw id. */
	class SArcLoreLockRow : public SMultiColumnTableRow<TSharedPtr<FArcLoreLockEntry>>
	{
	public:
		SLATE_BEGIN_ARGS(SArcLoreLockRow) {}
			SLATE_ARGUMENT(TSharedPtr<FArcLoreLockEntry>, Item)
			SLATE_ARGUMENT(FString, MyUserId)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
		{
			Item = InArgs._Item;
			MyUserId = InArgs._MyUserId;
			SMultiColumnTableRow<TSharedPtr<FArcLoreLockEntry>>::Construct(
				FSuperRowType::FArguments(), InOwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			if (!Item.IsValid())
			{
				return SNew(STextBlock);
			}

			if (InColumnName == ColumnLockPath)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->Path))
					.ToolTipText(FText::FromString(Item->Path));
			}

			if (InColumnName == ColumnLockOwner)
			{
				const bool bMine = !MyUserId.IsEmpty() && Item->OwnerId == MyUserId;
				return SNew(STextBlock)
					.Text(FText::FromString(bMine ? TEXT("me") : Item->OwnerId))
					.Font(bMine
						? FAppStyle::GetFontStyle("BoldFont")
						: FAppStyle::GetFontStyle("NormalFont"));
			}

			// ColumnLockAcquired (default)
			return SNew(STextBlock)
				.Text(FText::FromString(Item->LockedAt.ToString()))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground());
		}

	private:
		TSharedPtr<FArcLoreLockEntry> Item;
		FString MyUserId;
	};

	/** Build a fresh column header row for the Locks list. */
	TSharedRef<SHeaderRow> MakeLocksHeaderRow()
	{
		return SNew(SHeaderRow)
			+ SHeaderRow::Column(ColumnLockPath)
			.DefaultLabel(LOCTEXT("ColumnPath", "Path"))
			.FillWidth(0.55f)

			+ SHeaderRow::Column(ColumnLockOwner)
			.DefaultLabel(LOCTEXT("ColumnLockOwner", "Owner"))
			.FillWidth(0.2f)

			+ SHeaderRow::Column(ColumnLockAcquired)
			.DefaultLabel(LOCTEXT("ColumnLockAcquired", "Acquired"))
			.FillWidth(0.25f);
	}
}

void SArcLoreSourceControlPanel::ConstructFourSectionBody()
{
	using namespace ArcLorePanel_Internal;

	// All list views are created in Construct before this runs (this is called from
	// Construct in FourSection mode). Build a single Changes view: shared header rows,
	// a toolbar, a 2x2 grid (Dirty / Uncontrolled on top; Staged / Outgoing below) and
	// the commit block. No internal tab bar, History, Repositories or Locks.
	ChildSlot
	[
		SNew(SVerticalBox)

		// ---- Shared header ----

		// Header (branch / remote / identity / root)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcLoreSourceControlPanel::GetHeaderText)
		]

		// Identity row: signed-in status + Login / Logout buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SArcLoreSourceControlPanel::GetIdentityText)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Login", "Login"))
				.ToolTipText(LOCTEXT("LoginTip", "Sign in via browser OAuth."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanLogin)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnLoginInteractiveClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("LoginToken", "Token…"))
				.ToolTipText(LOCTEXT("LoginTokenTip", "Authenticate by pasting a Lore token."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanLogin)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnLoginClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Logout", "Logout"))
				.ToolTipText(LOCTEXT("LogoutTip", "Log out from Lore."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanLogout)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnLogoutClicked)
			]
		]

		// Local-commits-to-push summary
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcLoreSourceControlPanel::GetUnpushedSummaryText)
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SSeparator)
		]

		// ---- Toolbar: Refresh / Stage / Unstage / Sync-Pull / Revert ----
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Refresh", "Refresh"))
				.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnRefreshClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Stage", "Stage"))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanStage)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnStageClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Unstage", "Unstage"))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanUnstage)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnUnstageClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CleanStale", "Clean Stale"))
				.ToolTipText(LOCTEXT("CleanStaleTooltip", "Remove staged entries whose file no longer exists on disk (mark-for-add then deleted)."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnCleanStaleStagedClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SyncPull", "Sync / Pull"))
				.ToolTipText(LOCTEXT("SyncPullTip", "Fetch and integrate the latest changes from the remote (whole checkout)."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnSyncClicked)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Revert", "Revert"))
				.ToolTipText(LOCTEXT("RevertTip", "Discard local changes to the selected dirty file(s). This cannot be undone."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanRevertSelected)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnRevertClicked)
			]

			// Sort controls (key dropdown + direction toggle) — second sort entry-point
			// alongside the clickable column headers; both drive the shared SortKey/SortMode.
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				MakeSortControls()
			]
		]

		// ---- Body-vs-commit draggable boundary: a vertical splitter holds the 2x2 grid
		//      (top, ~0.7) and the commit block (bottom, ~0.3) so the user can resize them. ----
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			// TOP slot: the 2x2 grid (Dirty | Uncontrolled  /  Staged | Outgoing)
			+ SSplitter::Slot()
			.Value(0.7f)
			[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)

			// TOP row: Dirty | Uncontrolled
			+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				// Dirty (tracked changes)
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("DirtyTrackedSection", "Dirty (tracked changes)"))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						DirtyListView.ToSharedRef()
					]
				]

				// Uncontrolled (on disk, not in Lore)
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return FText::Format(
								LOCTEXT("UncontrolledSectionCount", "Uncontrolled ({0}) — on disk, not in Lore"),
								FText::AsNumber(UncontrolledItems.Num()));
						})
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						// At/below the limit: show the list. Above it: a count summary instead
						// (rendering thousands of rows is slow). Both read the live count via
						// lambdas — UncontrolledItems is still built in full by RefreshFileLists.
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(SBox)
							.Visibility_Lambda([this]() -> EVisibility
							{
								return UncontrolledItems.Num() <= kUncontrolledDisplayLimit
									? EVisibility::Visible : EVisibility::Collapsed;
							})
							[
								UncontrolledListView.ToSharedRef()
							]
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SNew(STextBlock)
							.AutoWrapText(true)
							.Visibility_Lambda([this]() -> EVisibility
							{
								return UncontrolledItems.Num() > kUncontrolledDisplayLimit
									? EVisibility::Visible : EVisibility::Collapsed;
							})
							.Text_Lambda([this]() -> FText
							{
								return FText::Format(
									LOCTEXT("UncontrolledTooMany", "Too many uncontrolled files to list ({0}). Click Refresh or narrow scope to view."),
									FText::AsNumber(UncontrolledItems.Num()));
							})
						]
					]
				]
			]

			// BOTTOM row: Staged | Outgoing
			+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)

				// Staged
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StagedSection", "Staged"))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						StagedListView.ToSharedRef()
					]
				]

				// Outgoing (committed-but-unpushed)
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OutgoingSection", "Outgoing — not pushed"))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]

					// Destructive unpushed-history-rewrite toolbar (confirm-gated).
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 2.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("UndoLastCommit", "Undo last commit"))
							.ToolTipText(LOCTEXT("UndoLastCommitTip", "Undo the last local commit; its files return to staged/dirty. Rewrites unpushed local history."))
							.IsEnabled(this, &SArcLoreSourceControlPanel::CanUndoLastCommit)
							.OnClicked(this, &SArcLoreSourceControlPanel::OnUndoLastCommitClicked)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("RemoveFromOutgoing", "Remove from commit"))
							.ToolTipText(LOCTEXT("RemoveFromOutgoingTip", "Remove the selected file from the last local commit. Rewrites unpushed history and re-commits."))
							.IsEnabled(this, &SArcLoreSourceControlPanel::CanRemoveFromOutgoing)
							.OnClicked(this, &SArcLoreSourceControlPanel::OnRemoveFromOutgoingClicked)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("AmendLastCommit", "Amend message..."))
							.ToolTipText(LOCTEXT("AmendLastCommitTip", "Amend the last local commit's message. Rewrites unpushed history."))
							.IsEnabled(this, &SArcLoreSourceControlPanel::CanAmendLastCommit)
							.OnClicked(this, &SArcLoreSourceControlPanel::OnAmendLastCommitClicked)
						]
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						OutgoingTreeView.ToSharedRef()
					]
				]
			]
			] // end inner vertical SSplitter (2x2 grid) + its TOP outer slot

			// BOTTOM slot: the commit block (Summary + Description + commit buttons).
			+ SSplitter::Slot()
			.Value(0.3f)
			[
			SNew(SVerticalBox)

			// Summary (subject — required)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CommitSummaryLabel", "Summary"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SAssignNew(CommitSummaryBox, SEditableTextBox)
				.HintText(LOCTEXT("CommitSummaryHint", "Summary (required)..."))
				.OnTextChanged_Lambda([this](const FText& InText)
				{
					CommitSummary = InText;
				})
			]

			// Description (body — optional)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CommitDescriptionLabel", "Description"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			// Description fills the commit slot so dragging the body-vs-commit splitter
			// resizes it (no fixed HeightOverride).
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SAssignNew(CommitDescriptionBox, SMultiLineEditableTextBox)
				.HintText(LOCTEXT("CommitDescriptionHint", "Description (optional)..."))
				.OnTextChanged_Lambda([this](const FText& InText)
				{
					CommitDescription = InText;
				})
			]

			// Commit buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Commit", "Commit"))
					.IsEnabled(this, &SArcLoreSourceControlPanel::CanCommit)
					.OnClicked(this, &SArcLoreSourceControlPanel::OnCommitClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CommitAndPush", "Commit & Push"))
					.IsEnabled(this, &SArcLoreSourceControlPanel::CanCommit)
					.OnClicked(this, &SArcLoreSourceControlPanel::OnCommitAndPushClicked)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Push", "Push"))
					.IsEnabled(this, &SArcLoreSourceControlPanel::CanPush)
					.OnClicked(this, &SArcLoreSourceControlPanel::OnPushClicked)
				]
			]
			] // end commit-block BOTTOM SSplitter::Slot
		] // end body-vs-commit vertical SSplitter (the body FillHeight slot)
	];
}

void SArcLoreSourceControlPanel::Construct(const FArguments& InArgs)
{
	using namespace ArcLorePanel_Internal;

	LayoutMode = InArgs._LayoutMode;

	FArcLoreSourceControlProvider& Provider = GetProvider();

	// Populate token-type combo-box options once (static list, matches the server's accepted types).
	TokenTypeOptions.Add(MakeShared<FString>(TEXT("lore")));
	TokenTypeOptions.Add(MakeShared<FString>(TEXT("api-key")));
	TokenTypeOptions.Add(MakeShared<FString>(TEXT("eg1")));
	SelectedTokenType = TokenTypeOptions[0];

	// Populate the toolbar sort-key dropdown options once (matches the three sort keys), and
	// seed the current selection from the active SortKey (defaults to "Name").
	SortKeyOptions.Add(MakeShared<FString>(TEXT("Name")));
	SortKeyOptions.Add(MakeShared<FString>(TEXT("Path")));
	SortKeyOptions.Add(MakeShared<FString>(TEXT("Type")));
	const FString InitialSortLabel = LabelForSortKey(SortKey);
	for (const TSharedPtr<FString>& Option : SortKeyOptions)
	{
		if (Option.IsValid() && *Option == InitialSortLabel)
		{
			SelectedSortKey = Option;
			break;
		}
	}

	DirtyListView = SNew(SListView<TSharedPtr<FArcLorePanelFileItem>>)
		.ListItemsSource(&DirtyItems)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateDirtyRow)
		.OnContextMenuOpening(this, &SArcLoreSourceControlPanel::OnDirtyContextMenuOpening)
		.HeaderRow(MakeFileHeaderRow());

	StagedListView = SNew(SListView<TSharedPtr<FArcLorePanelFileItem>>)
		.ListItemsSource(&StagedItems)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateStagedRow)
		.OnContextMenuOpening(this, &SArcLoreSourceControlPanel::OnStagedContextMenuOpening)
		.HeaderRow(MakeFileHeaderRow());

	UncontrolledListView = SNew(SListView<TSharedPtr<FArcLorePanelFileItem>>)
		.ListItemsSource(&UncontrolledItems)
		.SelectionMode(ESelectionMode::Multi)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateUncontrolledRow)
		.OnContextMenuOpening(this, &SArcLoreSourceControlPanel::OnUncontrolledContextMenuOpening)
		.HeaderRow(MakeFileHeaderRow());

	// Outgoing as a TREE grouped by commit (commit node -> its changed-file children).
	SAssignNew(OutgoingTreeView, STreeView<TSharedPtr<FArcLoreOutgoingNode>>)
		.TreeItemsSource(&OutgoingRoots)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateOutgoingRow)
		.OnGetChildren(this, &SArcLoreSourceControlPanel::OnGetOutgoingChildren)
		.OnContextMenuOpening(this, &SArcLoreSourceControlPanel::OnOutgoingContextMenuOpening)
		.HeaderRow(MakeOutgoingHeaderRow());

	HistoryListView = SNew(SListView<TSharedRef<FArcLoreSourceControlRevision>>)
		.ListItemsSource(&HistoryItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateHistoryRow)
		.OnSelectionChanged(this, &SArcLoreSourceControlPanel::OnHistorySelectionChanged)
		.HeaderRow(MakeCommitHeaderRow());

	// Sub-list under History: the files changed in the selected revision. A FLAT list
	// rendered by SArcLoreOutgoingRow (Icon / Name / Path) — its own 3-column header.
	SAssignNew(RevisionFilesView, SListView<TSharedPtr<FArcLoreUnpushedFile>>)
		.ListItemsSource(&RevisionFileItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateRevisionFileRow)
		.HeaderRow(MakeRevisionFileHeaderRow());

	BranchListView = SNew(SListView<TSharedPtr<FArcLoreBranchEntry>>)
		.ListItemsSource(&BranchItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateBranchRow)
		.HeaderRow(MakeBranchHeaderRow());

	InstanceListView = SNew(SListView<TSharedPtr<FArcLoreInstanceEntry>>)
		.ListItemsSource(&InstanceItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateInstanceRow)
		.HeaderRow(MakeInstanceHeaderRow());

	RemoteRepoListView = SNew(SListView<TSharedPtr<FArcLoreRepoEntry>>)
		.ListItemsSource(&RemoteRepoItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateRemoteRepoRow)
		.HeaderRow(MakeRepoHeaderRow());

	LockListView = SNew(SListView<TSharedPtr<FArcLoreLockEntry>>)
		.ListItemsSource(&LockItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SArcLoreSourceControlPanel::OnGenerateLockRow)
		.HeaderRow(MakeLocksHeaderRow());

	if (LayoutMode == EArcLoreChangesLayout::FourSection)
	{
		ConstructFourSectionBody();
	}
	else
	{
	ChildSlot
	[
		SNew(SVerticalBox)

		// ---- Shared header (always visible above the tabs) ----

		// Header (branch / remote / identity / root)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcLoreSourceControlPanel::GetHeaderText)
		]

		// Identity row: signed-in status + Login / Logout buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(SHorizontalBox)

			// Identity label (updates dynamically)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(this, &SArcLoreSourceControlPanel::GetIdentityText)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			// Login button (browser OAuth) — always shown; enabled only when idle
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Login", "Login"))
				.ToolTipText(LOCTEXT("LoginTip", "Sign in via browser OAuth."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanLogin)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnLoginInteractiveClicked)
			]

			// Token login button — opens the token-paste modal
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("LoginToken", "Token…"))
				.ToolTipText(LOCTEXT("LoginTokenTip", "Authenticate by pasting a Lore token."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanLogin)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnLoginClicked)
			]

			// Logout button — shown always; enabled only when idle and auth is resolved
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Logout", "Logout"))
				.ToolTipText(LOCTEXT("LogoutTip", "Log out from Lore."))
				.IsEnabled(this, &SArcLoreSourceControlPanel::CanLogout)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnLogoutClicked)
			]
		]

		// Local-commits-to-push summary
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SArcLoreSourceControlPanel::GetUnpushedSummaryText)
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SNew(SSeparator)
		]

		// ---- Tab bar (Changes / History / Repositories) ----
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 2.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("TabChanges", "Changes"))
				.ButtonColorAndOpacity(this, &SArcLoreSourceControlPanel::GetTabButtonColor, 0)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnTabClicked, 0)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("TabHistory", "History"))
				.ButtonColorAndOpacity(this, &SArcLoreSourceControlPanel::GetTabButtonColor, 1)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnTabClicked, 1)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("TabRepositories", "Repositories"))
				.ButtonColorAndOpacity(this, &SArcLoreSourceControlPanel::GetTabButtonColor, 2)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnTabClicked, 2)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("TabLocks", "Locks"))
				.ButtonColorAndOpacity(this, &SArcLoreSourceControlPanel::GetTabButtonColor, 3)
				.OnClicked(this, &SArcLoreSourceControlPanel::OnTabClicked, 3)
			]
		]

		// ---- Body: imperatively-switched tab content ----
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(TabSwitcher, SWidgetSwitcher)

			// Slot 0 — Changes: a top toolbar (Refresh/Stage/Unstage), a resizable
			// splitter body (Dirty on top; Staged | Outgoing side-by-side below), and
			// a bottom commit block (Summary + Description + commit buttons).
			+ SWidgetSwitcher::Slot()
			[
				SNew(SVerticalBox)

				// Slot A — top toolbar: Refresh / Stage / Unstage only.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Refresh", "Refresh"))
						.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnRefreshClicked)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Stage", "Stage"))
						.IsEnabled(this, &SArcLoreSourceControlPanel::CanStage)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnStageClicked)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Unstage", "Unstage"))
						.IsEnabled(this, &SArcLoreSourceControlPanel::CanUnstage)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnUnstageClicked)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("CleanStale", "Clean Stale"))
						.ToolTipText(LOCTEXT("CleanStaleTooltip", "Remove staged entries whose file no longer exists on disk (mark-for-add then deleted)."))
						.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnCleanStaleStagedClicked)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("SyncPull", "Sync / Pull"))
						.ToolTipText(LOCTEXT("SyncPullTip", "Fetch and integrate the latest changes from the remote (whole checkout)."))
						.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnSyncClicked)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Revert", "Revert"))
						.ToolTipText(LOCTEXT("RevertTip", "Discard local changes to the selected dirty file(s). This cannot be undone."))
						.IsEnabled(this, &SArcLoreSourceControlPanel::CanRevertSelected)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnRevertClicked)
					]

					// Sort controls (key dropdown + direction toggle) — second sort entry-point
					// alongside the clickable column headers; both drive the shared SortKey/SortMode.
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						MakeSortControls()
					]
				]

				// Slot B — resizable body: Dirty on top, then Staged | Outgoing.
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(4.0f)
				[
					SNew(SSplitter)
					.Orientation(Orient_Vertical)

					// Dirty (unstaged changes)
					+ SSplitter::Slot()
					.Value(0.4f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 2.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DirtySection", "Dirty (unstaged changes)"))
							.Font(FAppStyle::GetFontStyle("BoldFont"))
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							DirtyListView.ToSharedRef()
						]
					]

					// Staged | Outgoing side-by-side
					+ SSplitter::Slot()
					.Value(0.6f)
					[
						SNew(SSplitter)
						.Orientation(Orient_Horizontal)

						// Staged
						+ SSplitter::Slot()
						.Value(0.5f)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("StagedSection", "Staged"))
								.Font(FAppStyle::GetFontStyle("BoldFont"))
							]

							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								StagedListView.ToSharedRef()
							]
						]

						// Outgoing (committed-but-unpushed)
						+ SSplitter::Slot()
						.Value(0.5f)
						[
							SNew(SVerticalBox)

							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("OutgoingSection", "Outgoing — not pushed"))
								.Font(FAppStyle::GetFontStyle("BoldFont"))
							]

							// Destructive unpushed-history-rewrite toolbar (confirm-gated):
							// undo the last local commit, or drop one file from it.
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 0.0f, 0.0f, 2.0f)
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(0.0f, 0.0f, 2.0f, 0.0f)
								[
									SNew(SButton)
									.Text(LOCTEXT("UndoLastCommit", "Undo last commit"))
									.ToolTipText(LOCTEXT("UndoLastCommitTip", "Undo the last local commit; its files return to staged/dirty. Rewrites unpushed local history."))
									.IsEnabled(this, &SArcLoreSourceControlPanel::CanUndoLastCommit)
									.OnClicked(this, &SArcLoreSourceControlPanel::OnUndoLastCommitClicked)
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(2.0f, 0.0f)
								[
									SNew(SButton)
									.Text(LOCTEXT("RemoveFromOutgoing", "Remove from commit"))
									.ToolTipText(LOCTEXT("RemoveFromOutgoingTip", "Remove the selected file from the last local commit. Rewrites unpushed history and re-commits."))
									.IsEnabled(this, &SArcLoreSourceControlPanel::CanRemoveFromOutgoing)
									.OnClicked(this, &SArcLoreSourceControlPanel::OnRemoveFromOutgoingClicked)
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(2.0f, 0.0f)
								[
									SNew(SButton)
									.Text(LOCTEXT("AmendLastCommit", "Amend message..."))
									.ToolTipText(LOCTEXT("AmendLastCommitTip", "Amend the last local commit's message. Rewrites unpushed history."))
									.IsEnabled(this, &SArcLoreSourceControlPanel::CanAmendLastCommit)
									.OnClicked(this, &SArcLoreSourceControlPanel::OnAmendLastCommitClicked)
								]
							]

							+ SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								OutgoingTreeView.ToSharedRef()
							]
						]
					]
				]

				// Slot C — commit block: Summary + Description + commit buttons.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SVerticalBox)

					// Summary (subject — required)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CommitSummaryLabel", "Summary"))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SAssignNew(CommitSummaryBox, SEditableTextBox)
						.HintText(LOCTEXT("CommitSummaryHint", "Summary (required)..."))
						.OnTextChanged_Lambda([this](const FText& InText)
						{
							CommitSummary = InText;
						})
					]

					// Description (body — optional)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CommitDescriptionLabel", "Description"))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(SBox)
						.HeightOverride(80.0f)
						[
							SAssignNew(CommitDescriptionBox, SMultiLineEditableTextBox)
							.HintText(LOCTEXT("CommitDescriptionHint", "Description (optional)..."))
							.OnTextChanged_Lambda([this](const FText& InText)
							{
								CommitDescription = InText;
							})
						]
					]

					// Commit buttons
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("Commit", "Commit"))
							.IsEnabled(this, &SArcLoreSourceControlPanel::CanCommit)
							.OnClicked(this, &SArcLoreSourceControlPanel::OnCommitClicked)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("CommitAndPush", "Commit & Push"))
							.IsEnabled(this, &SArcLoreSourceControlPanel::CanCommit)
							.OnClicked(this, &SArcLoreSourceControlPanel::OnCommitAndPushClicked)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("Push", "Push"))
							.IsEnabled(this, &SArcLoreSourceControlPanel::CanPush)
							.OnClicked(this, &SArcLoreSourceControlPanel::OnPushClicked)
						]
					]
				]
			]

			// Slot 1 — History (inline Refresh button above the list).
			+ SWidgetSwitcher::Slot()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SImage)
						.Image(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.Actions.History"))
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("HistorySection", "History"))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("HistoryRefresh", "Refresh"))
						.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
						.OnClicked_Lambda([this]() -> FReply
						{
							RefreshHistory();
							return FReply::Handled();
						})
					]
				]

				// Split the History tab three ways: commit list on top, the selected
				// revision's full commit message in the middle, and its changed files
				// (with a caption) on the bottom.
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(4.0f)
				[
					SNew(SSplitter)
					.Orientation(Orient_Vertical)

					+ SSplitter::Slot()
					.Value(0.5f)
					[
						HistoryListView.ToSharedRef()
					]

					// Full commit message of the selected revision (read-only).
					+ SSplitter::Slot()
					.Value(0.25f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 4.0f, 0.0f, 2.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("RevisionMessageLabel", "Commit message"))
							.Font(FAppStyle::GetFontStyle("BoldFont"))
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							SAssignNew(RevisionMessageBox, SMultiLineEditableTextBox)
							.IsReadOnly(true)
							.AutoWrapText(true)
						]
					]

					+ SSplitter::Slot()
					.Value(0.25f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 4.0f, 0.0f, 2.0f)
						[
							SAssignNew(RevisionFilesHeader, STextBlock)
							.Text(LOCTEXT("RevisionFilesNone", "Select a revision to see its files"))
							.Font(FAppStyle::GetFontStyle("BoldFont"))
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						[
							RevisionFilesView.ToSharedRef()
						]
					]
				]
			]

			// Slot 2 — Repositories (branches / instances / remote repos). Branches+Instances
			// come from the LOCAL FLoreRepositoryView op (refreshed on the panel's normal
			// Refresh); the Remote list is populated ONLY by the explicit button
			// (FLoreListRemoteRepos blocks on the network).
			+ SWidgetSwitcher::Slot()
			[
				SNew(SVerticalBox)

				// Branches
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BranchesLabel", "Branches"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]

				// Branch toolbar (Switch / Create branch).
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 2.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 2.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("SwitchBranch", "Switch"))
						.ToolTipText(LOCTEXT("SwitchBranchTip", "Switch the working copy to the selected branch. Prompts if staged or local changes are present."))
						.IsEnabled(this, &SArcLoreSourceControlPanel::CanSwitchBranch)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnSwitchBranchClicked)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(2.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("CreateBranch", "Create branch..."))
						.ToolTipText(LOCTEXT("CreateBranchTip", "Create a new branch from the current revision."))
						.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnCreateBranchClicked)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SBorder)
					.BorderImage(FRevisionControlStyleManager::Get().GetBrush("RevisionControl.BranchExplorer.BranchList.Branch"))
					[
						BranchListView.ToSharedRef()
					]
				]

				// Instances
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("InstancesLabel", "Instances"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					InstanceListView.ToSharedRef()
				]

				// Remote repositories (button-driven — never auto-dispatched)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RemoteLabel", "Remote"))
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ListRemoteRepos", "List remote repositories..."))
					.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
					.OnClicked(this, &SArcLoreSourceControlPanel::OnListRemoteReposClicked)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					RemoteRepoListView.ToSharedRef()
				]
			]

			// Slot 3 — Locks: a Refresh button + an Unlock button (enabled when a lock
			// row is selected) over the branch-wide lock list.
			+ SWidgetSwitcher::Slot()
			[
				SNew(SVerticalBox)

				// Action toolbar: Refresh / Unlock.
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("RefreshLocks", "Refresh"))
						.ToolTipText(LOCTEXT("RefreshLocksTip", "List all locks on the current branch."))
						.IsEnabled(this, &SArcLoreSourceControlPanel::IsIdle)
						.OnClicked_Lambda([this]() -> FReply
						{
							RefreshLocks();
							return FReply::Handled();
						})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SButton)
						.Text(LOCTEXT("ReleaseLock", "Unlock"))
						.ToolTipText(LOCTEXT("ReleaseLockTip", "Release the selected lock (without pushing)."))
						.IsEnabled(this, &SArcLoreSourceControlPanel::CanReleaseSelectedLock)
						.OnClicked(this, &SArcLoreSourceControlPanel::OnReleaseSelectedLockClicked)
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					LockListView.ToSharedRef()
				]
			]
		]
	];

	// Start on the Changes tab.
	if (TabSwitcher.IsValid())
	{
		TabSwitcher->SetActiveWidgetIndex(ActiveTabIndex);
	}
	} // end legacy-tabbed branch

	StateChangedHandle = Provider.RegisterSourceControlStateChanged_Handle(
		FSourceControlStateChanged::FDelegate::CreateSP(this, &SArcLoreSourceControlPanel::OnSourceControlStateChanged));

	// Show the cached set instantly (good perceived latency).
	RefreshFileLists();

	// Authoritative repo-wide refresh on open — BOTH layouts. The repo-wide UpdateStatus
	// pass enumerates every dirty/staged/untracked file and resets every OTHER cached entry
	// to clean (FArcLoreUpdateStatusWorker::UpdateStates), so a file committed since the last
	// refresh (e.g. PCG_Forest) drops out of the Dirty list on open WITHOUT a manual Refresh.
	// Unguarded to match OnRefreshClicked; the op is async, so a disconnected editor just gets
	// a pending op rather than a UI stall. (Also repopulates Outgoing.)
	RefreshAllRepoWide();

	if (LayoutMode != EArcLoreChangesLayout::FourSection)
	{
		// The legacy tabbed layout also hosts History / Repositories / Locks views.
		RefreshHistory();
		RefreshRepositories();
		RefreshLocks();
	}
}

SArcLoreSourceControlPanel::~SArcLoreSourceControlPanel()
{
	// Guard against provider/module teardown ordering (cf. engine source-control widgets).
	if (StateChangedHandle.IsValid() && FModuleManager::Get().IsModuleLoaded("SourceControl"))
	{
		ISourceControlProvider& BaseProvider = ISourceControlModule::Get().GetProvider();
		BaseProvider.UnregisterSourceControlStateChanged_Handle(StateChangedHandle);
		StateChangedHandle.Reset();
	}
}

FArcLoreSourceControlProvider& SArcLoreSourceControlPanel::GetProvider() const
{
	// Mirror SArcLoreSourceControlSettings: the ArcLore module owns the provider instance.
	FArcLoreSourceControlModule& Module = FModuleManager::GetModuleChecked<FArcLoreSourceControlModule>("ArcLore");
	return Module.GetProvider();
}

TSharedRef<SHeaderRow> SArcLoreSourceControlPanel::MakeFileHeaderRow()
{
	using namespace ArcLorePanel_Internal;

	return SNew(SHeaderRow)
		+ SHeaderRow::Column(ColumnIcon)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(24.0f)

		+ SHeaderRow::Column(ColumnName)
		.DefaultLabel(LOCTEXT("ColumnName", "Name"))
		.FillWidth(0.3f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnName)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader)

		+ SHeaderRow::Column(ColumnLock)
		.DefaultLabel(LOCTEXT("ColumnLock", "Lock"))
		.FillWidth(0.2f)

		+ SHeaderRow::Column(ColumnType)
		.DefaultLabel(LOCTEXT("ColumnType", "Type"))
		.FillWidth(0.2f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnType)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader)

		+ SHeaderRow::Column(ColumnPath)
		.DefaultLabel(LOCTEXT("ColumnPath", "Path"))
		.FillWidth(0.3f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnPath)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader);
}

TSharedRef<SHeaderRow> SArcLoreSourceControlPanel::MakeOutgoingHeaderRow()
{
	using namespace ArcLorePanel_Internal;

	return SNew(SHeaderRow)
		+ SHeaderRow::Column(ColumnOutgoingName)
		.DefaultLabel(LOCTEXT("ColumnName", "Name"))
		.FillWidth(0.6f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnOutgoingName)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader)

		+ SHeaderRow::Column(ColumnOutgoingType)
		.DefaultLabel(LOCTEXT("ColumnOutgoingType", "Type"))
		.FillWidth(0.2f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnOutgoingType)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader)

		+ SHeaderRow::Column(ColumnOutgoingPath)
		.DefaultLabel(LOCTEXT("ColumnPath", "Path"))
		.FillWidth(0.3f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnOutgoingPath)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader);
}

TSharedRef<SHeaderRow> SArcLoreSourceControlPanel::MakeRevisionFileHeaderRow()
{
	using namespace ArcLorePanel_Internal;

	return SNew(SHeaderRow)
		+ SHeaderRow::Column(ColumnOutgoingIcon)
		.DefaultLabel(FText::GetEmpty())
		.FixedWidth(24.0f)

		+ SHeaderRow::Column(ColumnOutgoingName)
		.DefaultLabel(LOCTEXT("ColumnName", "Name"))
		.FillWidth(0.4f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnOutgoingName)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader)

		+ SHeaderRow::Column(ColumnOutgoingType)
		.DefaultLabel(LOCTEXT("ColumnOutgoingType", "Type"))
		.FillWidth(0.2f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnOutgoingType)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader)

		+ SHeaderRow::Column(ColumnOutgoingPath)
		.DefaultLabel(LOCTEXT("ColumnPath", "Path"))
		.FillWidth(0.4f)
		.SortMode(this, &SArcLoreSourceControlPanel::GetSortModeForColumn, ColumnOutgoingPath)
		.OnSort(this, &SArcLoreSourceControlPanel::OnSortColumnHeader);
}

TSharedRef<SWidget> SArcLoreSourceControlPanel::MakeSortControls()
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
		// firing OnSelectionChanged (no spurious construct-time sort); the OnSelectionChanged
		// guard ignores Direct selections (SyncSortCombo's SetSelectedItem) to break the
		// header<->dropdown feedback loop. Stored into the single SortKeyCombo member — only one
		// toolbar is live per LayoutMode, so whichever build path runs owns it.
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2.0f, 0.0f)
		[
			SAssignNew(SortKeyCombo, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&SortKeyOptions)
			.InitiallySelectedItem(SelectedSortKey)
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> InValue, ESelectInfo::Type SelectInfo) -> void
			{
				if (InValue.IsValid() && SelectInfo != ESelectInfo::Direct)
				{
					SelectedSortKey = InValue;
					SortKey = SortKeyFromLabel(*InValue);
					ApplySortToAllLists();
				}
			})
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
			{
				return SNew(STextBlock)
					.Text(InItem.IsValid() ? FText::FromString(*InItem) : FText::GetEmpty());
			})
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
			.OnClicked(this, &SArcLoreSourceControlPanel::OnToggleSortDirectionClicked)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::FromString(SortMode == EColumnSortMode::Descending ? TEXT("v") : TEXT("^"));
				})
			]
		];
}

SArcLoreSourceControlPanel::EArcLoreSortKey SArcLoreSourceControlPanel::KeyForColumn(const FName& Col) const
{
	using namespace ArcLorePanel_Internal;
	if (Col == ColumnPath || Col == ColumnOutgoingPath)
	{
		return EArcLoreSortKey::Path;
	}
	if (Col == ColumnType || Col == ColumnOutgoingType)
	{
		return EArcLoreSortKey::Type;
	}
	// ColumnName / ColumnOutgoingName (and any non-sortable column) fold onto Name.
	return EArcLoreSortKey::Name;
}

EColumnSortMode::Type SArcLoreSourceControlPanel::GetSortModeForColumn(FName Col) const
{
	return KeyForColumn(Col) == SortKey ? SortMode : EColumnSortMode::None;
}

void SArcLoreSourceControlPanel::OnSortColumnHeader(EColumnSortPriority::Type SortPriority, const FName& Col, EColumnSortMode::Type NewMode)
{
	SortKey = KeyForColumn(Col);
	SortMode = NewMode;
	SyncSortCombo();
	ApplySortToAllLists();
}

void SArcLoreSourceControlPanel::ApplySortToAllLists()
{
	SortChangesLists();
	SortRevisionFiles();
	SortOutgoingTree();
}

SArcLoreSourceControlPanel::EArcLoreSortKey SArcLoreSourceControlPanel::SortKeyFromLabel(const FString& Label) const
{
	if (Label == TEXT("Path"))
	{
		return EArcLoreSortKey::Path;
	}
	if (Label == TEXT("Type"))
	{
		return EArcLoreSortKey::Type;
	}
	return EArcLoreSortKey::Name;
}

FString SArcLoreSourceControlPanel::LabelForSortKey(EArcLoreSortKey Key) const
{
	switch (Key)
	{
	case EArcLoreSortKey::Path:
		return TEXT("Path");
	case EArcLoreSortKey::Type:
		return TEXT("Type");
	case EArcLoreSortKey::Name:
	default:
		return TEXT("Name");
	}
}

void SArcLoreSourceControlPanel::SyncSortCombo()
{
	const FString ActiveLabel = LabelForSortKey(SortKey);
	for (const TSharedPtr<FString>& Option : SortKeyOptions)
	{
		if (Option.IsValid() && *Option == ActiveLabel)
		{
			SelectedSortKey = Option;
			break;
		}
	}
	if (SortKeyCombo.IsValid() && SelectedSortKey.IsValid())
	{
		SortKeyCombo->SetSelectedItem(SelectedSortKey);
	}
}

FReply SArcLoreSourceControlPanel::OnToggleSortDirectionClicked()
{
	SortMode = (SortMode == EColumnSortMode::Descending)
		? EColumnSortMode::Ascending
		: EColumnSortMode::Descending;
	ApplySortToAllLists();
	return FReply::Handled();
}

void SArcLoreSourceControlPanel::SortChangesLists()
{
	using namespace ArcLorePanel_Internal;

	// Named lambdas (must outlive the TFunctionRef params bound from them).
	const TFunction<FString(const FArcLorePanelFileItem&)> GetAbs = [](const FArcLorePanelFileItem& Item) -> FString { return FileItemAbs(Item); };
	const TFunction<FString& (FArcLorePanelFileItem&)> GetTypeRef = [](FArcLorePanelFileItem& Item) -> FString& { return Item.CachedType; };
	const TFunction<bool& (FArcLorePanelFileItem&)> GetTypeResolvedRef = [](FArcLorePanelFileItem& Item) -> bool& { return Item.bTypeResolved; };

	ArcLoreAssetDisplay::SortFileItems<FArcLorePanelFileItem>(DirtyItems, SortKey, SortMode, GetAbs, GetTypeRef, GetTypeResolvedRef);
	ArcLoreAssetDisplay::SortFileItems<FArcLorePanelFileItem>(StagedItems, SortKey, SortMode, GetAbs, GetTypeRef, GetTypeResolvedRef);

	// Over the display cap the Uncontrolled list isn't rendered (a count summary shows
	// instead), so sorting/Type-resolving up to ~62k items would be pure game-thread waste.
	if (UncontrolledItems.Num() <= kUncontrolledDisplayLimit)
	{
		ArcLoreAssetDisplay::SortFileItems<FArcLorePanelFileItem>(UncontrolledItems, SortKey, SortMode, GetAbs, GetTypeRef, GetTypeResolvedRef);
	}

	if (DirtyListView.IsValid())
	{
		DirtyListView->RequestListRefresh();
	}
	if (StagedListView.IsValid())
	{
		StagedListView->RequestListRefresh();
	}
	if (UncontrolledListView.IsValid())
	{
		UncontrolledListView->RequestListRefresh();
	}
}

void SArcLoreSourceControlPanel::SortRevisionFiles()
{
	using namespace ArcLorePanel_Internal;

	// Named lambdas (must outlive the TFunctionRef params bound from them).
	const TFunction<FString(const FArcLoreUnpushedFile&)> GetAbs = [](const FArcLoreUnpushedFile& File) -> FString { return UnpushedAbs(File); };
	const TFunction<FString& (FArcLoreUnpushedFile&)> GetTypeRef = [](FArcLoreUnpushedFile& File) -> FString& { return File.CachedType; };
	const TFunction<bool& (FArcLoreUnpushedFile&)> GetTypeResolvedRef = [](FArcLoreUnpushedFile& File) -> bool& { return File.bTypeResolved; };

	ArcLoreAssetDisplay::SortFileItems<FArcLoreUnpushedFile>(RevisionFileItems, SortKey, SortMode, GetAbs, GetTypeRef, GetTypeResolvedRef);

	if (RevisionFilesView.IsValid())
	{
		RevisionFilesView->RequestListRefresh();
	}
}

void SArcLoreSourceControlPanel::SortOutgoingTree()
{
	using namespace ArcLorePanel_Internal;

	if (SortMode != EColumnSortMode::None)
	{
		// Each commit node's Children are file NODES; sort the node array in place by the
		// underlying File payload's precomputed keys (never extract->rebuild — keep the
		// node wrappers). Commit nodes themselves keep their chronological order.
		for (const TSharedPtr<FArcLoreOutgoingNode>& CommitNode : OutgoingRoots)
		{
			if (!CommitNode.IsValid())
			{
				continue;
			}

			// Pre-pass: resolve CachedType on every child's File so the comparator is pure.
			for (const TSharedPtr<FArcLoreOutgoingNode>& ChildNode : CommitNode->Children)
			{
				if (ChildNode.IsValid() && ChildNode->File.IsValid() && !ChildNode->File->bTypeResolved)
				{
					ChildNode->File->CachedType = ArcLoreAssetDisplay::ResolveAssetTypeName(UnpushedAbs(*ChildNode->File));
					ChildNode->File->bTypeResolved = true;
				}
			}

			const EArcLoreSortKey LocalKey = SortKey;
			const EColumnSortMode::Type LocalMode = SortMode;
			CommitNode->Children.Sort([LocalKey, LocalMode](const TSharedPtr<FArcLoreOutgoingNode>& A, const TSharedPtr<FArcLoreOutgoingNode>& B) -> bool
			{
				const FString AbsA = (A.IsValid() && A->File.IsValid()) ? UnpushedAbs(*A->File) : FString();
				const FString AbsB = (B.IsValid() && B->File.IsValid()) ? UnpushedAbs(*B->File) : FString();
				const FString TypeA = (A.IsValid() && A->File.IsValid()) ? A->File->CachedType : FString();
				const FString TypeB = (B.IsValid() && B->File.IsValid()) ? B->File->CachedType : FString();
				return ArcLoreAssetDisplay::SortKeyLess(
					FPaths::GetBaseFilename(AbsA), FPaths::GetPath(AbsA), TypeA,
					FPaths::GetBaseFilename(AbsB), FPaths::GetPath(AbsB), TypeB,
					LocalKey, LocalMode);
			});
		}
	}

	if (OutgoingTreeView.IsValid())
	{
		OutgoingTreeView->RequestTreeRefresh();
	}
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateDirtyRow(TSharedPtr<FArcLorePanelFileItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreFileRow, OwnerTable).Item(InItem);
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateStagedRow(TSharedPtr<FArcLorePanelFileItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreFileRow, OwnerTable).Item(InItem);
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateUncontrolledRow(TSharedPtr<FArcLorePanelFileItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreFileRow, OwnerTable).Item(InItem);
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateOutgoingRow(TSharedPtr<FArcLoreOutgoingNode> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreOutgoingTreeRow, OwnerTable).Item(InItem);
}

void SArcLoreSourceControlPanel::OnGetOutgoingChildren(TSharedPtr<FArcLoreOutgoingNode> InItem, TArray<TSharedPtr<FArcLoreOutgoingNode>>& OutChildren)
{
	if (InItem.IsValid())
	{
		OutChildren = InItem->Children;
	}
}

void SArcLoreSourceControlPanel::RefreshUnpushed()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	// No-file op; bind the op into the completion delegate so it stays alive and its
	// Files payload is readable on completion.
	TSharedRef<FLoreListUnpushed> Op = ISourceControlOperation::Create<FLoreListUnpushed>();
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnUnpushedComplete, Op));
}

void SArcLoreSourceControlPanel::OnUnpushedComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreListUnpushed> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	// Build the Outgoing tree eagerly: one commit node per unpushed commit (newest
	// first), one file child per commit's changed files. Each file node's AbsPath is
	// filled here (game thread) from the repo root + repo-relative path so the friendly-
	// name resolver can map the OFPA GUID filename to an asset/actor label.
	const FString RepoRoot = GetProvider().GetPathToRepositoryRoot();
	OutgoingRoots.Reset();
	OutgoingRoots.Reserve(InOp->Commits.Num());
	for (const FArcLoreUnpushedCommit& Commit : InOp->Commits)
	{
		TSharedPtr<FArcLoreOutgoingNode> CommitNode = MakeShared<FArcLoreOutgoingNode>();
		CommitNode->Commit = MakeShared<FArcLoreUnpushedCommit>(Commit);
		CommitNode->Children.Reserve(Commit.Files.Num());
		for (const FArcLoreUnpushedFile& File : Commit.Files)
		{
			TSharedPtr<FArcLoreUnpushedFile> FilePayload = MakeShared<FArcLoreUnpushedFile>(File);
			FilePayload->AbsPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(RepoRoot, File.Path));

			TSharedPtr<FArcLoreOutgoingNode> FileNode = MakeShared<FArcLoreOutgoingNode>();
			FileNode->File = FilePayload;
			CommitNode->Children.Add(FileNode);
		}
		OutgoingRoots.Add(CommitNode);
	}

	if (OutgoingTreeView.IsValid())
	{
		// Expand every commit node by default so its files are visible without a click.
		for (const TSharedPtr<FArcLoreOutgoingNode>& CommitNode : OutgoingRoots)
		{
			OutgoingTreeView->SetItemExpansion(CommitNode, true);
		}
	}

	// Sort each commit's file children by the active key/direction and refresh the tree
	// (SortOutgoingTree refreshes — replaces the inline RequestTreeRefresh).
	SortOutgoingTree();
}

bool SArcLoreSourceControlPanel::CanUndoLastCommit() const
{
	return IsIdle() && GetProvider().IsLocalAhead();
}

bool SArcLoreSourceControlPanel::CanRemoveFromOutgoing() const
{
	if (!IsIdle() || !GetProvider().IsLocalAhead() || !OutgoingTreeView.IsValid())
	{
		return false;
	}

	// Exactly one selected node, and it must be a FILE node (commit headers can't be
	// "removed from commit"). The worker only acts on the LATEST unpushed commit, but
	// the selection guard here is purely "one file node selected" — the worker's
	// soft-skip path reports a file that is not in the latest commit.
	const TArray<TSharedPtr<FArcLoreOutgoingNode>> Sel = OutgoingTreeView->GetSelectedItems();
	return Sel.Num() == 1 && Sel[0].IsValid() && Sel[0]->IsFile();
}

bool SArcLoreSourceControlPanel::CanAmendLastCommit() const
{
	return IsIdle() && GetProvider().IsLocalAhead();
}

FReply SArcLoreSourceControlPanel::OnUndoLastCommitClicked()
{
	// DESTRUCTIVE: rewrites unpushed local history. Confirm before dispatching.
	const EAppReturnType::Type Choice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("UndoConfirm", "Undo the last local commit? Its files return to staged/dirty. This rewrites unpushed local history."));
	if (Choice != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		ISourceControlOperation::Create<FLoreUndoLastCommit>(),
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

FReply SArcLoreSourceControlPanel::OnAmendLastCommitClicked()
{
	// Prefill with the latest unpushed commit's current message when readily reachable
	// (OutgoingRoots is newest-first), else leave empty — no extra query just for prefill.
	FString PrefillMessage;
	if (OutgoingRoots.Num() > 0 && OutgoingRoots[0].IsValid() && OutgoingRoots[0]->Commit.IsValid())
	{
		PrefillMessage = OutgoingRoots[0]->Commit->Message;
	}

	// Modal dialog with a multi-line message box (mirrors the Create-Branch modal).
	TSharedPtr<SMultiLineEditableTextBox> MessageBox;

	TSharedRef<SWindow> AmendWindow = SNew(SWindow)
		.Title(LOCTEXT("AmendWindowTitle", "Amend Last Commit Message"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	// Capture the window ptr so the OK/Cancel lambdas can close it.
	TSharedPtr<SWindow> AmendWindowPtr = AmendWindow;

	// Shared flag: true when the user confirmed (pressed OK).
	TSharedPtr<bool> bConfirmed = MakeShared<bool>(false);

	AmendWindow->SetContent(
		SNew(SBox)
		.MinDesiredWidth(420.0f)
		.Padding(FMargin(16.0f))
		[
			SNew(SVerticalBox)

			// Message label
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AmendMessageLabel", "New commit message"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			// Multi-line message editor (~6 lines tall).
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(SBox)
				.HeightOverride(120.0f)
				[
					SAssignNew(MessageBox, SMultiLineEditableTextBox)
					.Text(FText::FromString(PrefillMessage))
					.HintText(LOCTEXT("AmendMessageHint", "Enter the new commit message..."))
				]
			]

			// OK / Cancel row
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("AmendOK", "OK"))
					.OnClicked_Lambda([AmendWindowPtr, bConfirmed]() -> FReply
					{
						*bConfirmed = true;
						if (AmendWindowPtr.IsValid())
						{
							AmendWindowPtr->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("AmendCancel", "Cancel"))
					.OnClicked_Lambda([AmendWindowPtr]() -> FReply
					{
						if (AmendWindowPtr.IsValid())
						{
							AmendWindowPtr->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
			]
		]
	);

	// Block until the window is dismissed (modal).
	FSlateApplication::Get().AddModalWindow(AmendWindow, FSlateApplication::Get().GetActiveTopLevelWindow());

	// Window has been closed — read results.
	if (!(*bConfirmed))
	{
		return FReply::Handled();
	}

	const FString NewMessage = MessageBox.IsValid() ? MessageBox->GetText().ToString() : FString();
	if (NewMessage.TrimStartAndEnd().IsEmpty())
	{
		return FReply::Handled();
	}

	TSharedRef<FLoreAmendLastCommit> Op = ISourceControlOperation::Create<FLoreAmendLastCommit>();
	Op->NewMessage = NewMessage;

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

FReply SArcLoreSourceControlPanel::OnRemoveFromOutgoingClicked()
{
	if (!OutgoingTreeView.IsValid())
	{
		return FReply::Handled();
	}

	// Require exactly one selected FILE node; commit headers are not removable.
	const TArray<TSharedPtr<FArcLoreOutgoingNode>> Sel = OutgoingTreeView->GetSelectedItems();
	if (Sel.Num() != 1 || !Sel[0].IsValid() || !Sel[0]->IsFile() || !Sel[0]->File.IsValid())
	{
		return FReply::Handled();
	}

	const FString TargetPath = Sel[0]->File->Path;

	// DESTRUCTIVE: rewrites unpushed history and re-commits the remaining files as the
	// current identity with a new timestamp. Name the file and the re-author caveat.
	const EAppReturnType::Type Choice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::Format(
			LOCTEXT("RemoveFromOutgoingConfirm", "Remove '{0}' from the last commit? This rewrites unpushed history and re-commits as you with a new timestamp."),
			FText::FromString(TargetPath)));
	if (Choice != EAppReturnType::Yes)
	{
		return FReply::Handled();
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	TSharedRef<FLoreRemoveFromOutgoing> Op = ISourceControlOperation::Create<FLoreRemoveFromOutgoing>();
	Op->FilePath = TargetPath;

	bOperationInProgress = true;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateHistoryRow(TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreCommitRow, OwnerTable).Item(InItem);
}

void SArcLoreSourceControlPanel::RefreshAllRepoWide()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	// The repo-root sentinel puts the worker in repo-wide mode: it discovers ALL dirty and
	// untracked files project-wide (not just the editor's Content-Browser-scoped cache).
	// Async — must not block the UI; OnOperationComplete re-bins the lists when it returns.
	bOperationInProgress = true;
	const TArray<FString> RepoRootFiles = { Provider.GetPathToRepositoryRoot() };
	Provider.Execute(
		ISourceControlOperation::Create<FUpdateStatus>(),
		nullptr,
		RepoRootFiles,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	// Repopulate the Outgoing (committed-but-unpushed) list.
	RefreshUnpushed();
}

void SArcLoreSourceControlPanel::RefreshFileLists()
{
	using namespace ArcLorePanel_Internal;

	FArcLoreSourceControlProvider& Provider = GetProvider();

	const TArray<FSourceControlStateRef> States = Provider.GetCachedStateByPredicate(
		[](const FSourceControlStateRef& S) -> bool
		{
			const FArcLoreSourceControlState& Arc = static_cast<const FArcLoreSourceControlState&>(S.Get());
			// NotControlled (untracked) is included so discovered new files surface in
			// the panel; it bins into the Dirty section via IsDirtyUnstaged.
			// !IsCurrent() (remote-ahead) intentionally excluded — it's branch divergence,
			// not a local change (see IsDirtyUnstaged). It still drives the CB out-of-date overlay.
			return Arc.bStaged || S->IsModified() || S->IsDeleted()
				|| Arc.WorkingCopyState == EArcLoreState::NotControlled;
		});

	DirtyItems.Reset();
	StagedItems.Reset();
	UncontrolledItems.Reset();

	for (const FSourceControlStateRef& State : States)
	{
		const FArcLoreSourceControlState& Arc = static_cast<const FArcLoreSourceControlState&>(State.Get());

		if (IsStaged(State))
		{
			StagedItems.Add(MakeShared<FArcLorePanelFileItem>(State));
		}
		else if (Arc.WorkingCopyState == EArcLoreState::NotControlled)
		{
			// FourSection: untracked files get their own section; LegacyTabbed: they bin
			// into Dirty (so the user can still select + Stage them) as before.
			if (LayoutMode == EArcLoreChangesLayout::FourSection)
			{
				UncontrolledItems.Add(MakeShared<FArcLorePanelFileItem>(State));
			}
			else
			{
				DirtyItems.Add(MakeShared<FArcLorePanelFileItem>(State));
			}
		}
		else if (State->IsModified() || State->IsDeleted())
		{
			DirtyItems.Add(MakeShared<FArcLorePanelFileItem>(State));
		}
	}

	// Sort the freshly-binned changes lists by the active key/direction and refresh
	// each valid view (SortChangesLists refreshes — no separate RequestListRefresh trio).
	SortChangesLists();
}

void SArcLoreSourceControlPanel::OnSourceControlStateChanged()
{
	RefreshFileLists();
	RefreshLocks();
	RefreshUnpushed();
}

void SArcLoreSourceControlPanel::OnOperationComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	bOperationInProgress = false;
	RefreshFileLists();

	// A commit moves files Staged -> Outgoing and adds a History entry; a push clears
	// Outgoing and updates ahead/behind. Refresh those lists when such an op completes.
	// (Stage/Unstage don't change them, so they are skipped to avoid an extra remote
	// status round-trip on every staging click.)
	const FName OpName = InOperation->GetName();
	if (OpName == "CheckIn" || OpName == "Push"
		|| OpName == "LoreUndoLastCommit" || OpName == "LoreRemoveFromOutgoing"
		|| OpName == "LoreAmendLastCommit"
		|| OpName == "BranchSwitch")
	{
		RefreshUnpushed();
		RefreshHistory();
	}

	// A lock release (explicit Unlock), a Push (releases locks on the pushed files), or
	// an acquire (Lock) changes the branch-wide lock set — re-list so the Locks tab stays
	// current.
	if (OpName == "LoreReleaseLock" || OpName == "Push" || OpName == "CheckOut")
	{
		RefreshLocks();
	}

	// The per-file remove reports a soft skip (e.g. "not in the latest unpushed commit")
	// via bRemoved=false + a Note; surface it. A hard failure sets ErrorMessage instead,
	// which the provider already logs.
	if (OpName == "LoreRemoveFromOutgoing")
	{
		const TSharedRef<FLoreRemoveFromOutgoing> RemoveOp = StaticCastSharedRef<FLoreRemoveFromOutgoing>(InOperation);
		if (!RemoveOp->bRemoved && !RemoveOp->Note.IsEmpty())
		{
			UE_LOGF(LogSourceControl, Warning,"ArcLore: Remove from Outgoing skipped: %ls", *RemoveOp->Note);
		}
	}

	// A Lock (FCheckOut) failure means a selected file is held by another user (the worker
	// fails the whole acquire). The provider already logged the lore error naming the
	// file/owner; pop an explicit dialog so the user sees why the lock did not land.
	if (OpName == "CheckOut" && InResult != ECommandResult::Succeeded)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("LockFailed", "Could not lock the selected file(s) — one or more are held by another user. See the Output Log for details."));
	}
}

void SArcLoreSourceControlPanel::RefreshHistory()
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
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnHistoryComplete, Op));
}

void SArcLoreSourceControlPanel::OnHistoryComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRevisionLog> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	HistoryItems = InOp->Revisions;
	if (HistoryListView.IsValid())
	{
		HistoryListView->RequestListRefresh();
	}

	// The new revisions replace the old selection; clear the per-revision file sub-list
	// so it does not show files for a now-gone selection.
	RevisionFileItems.Reset();
	if (RevisionFilesView.IsValid())
	{
		RevisionFilesView->RequestListRefresh();
	}
	if (RevisionFilesHeader.IsValid())
	{
		RevisionFilesHeader->SetText(LOCTEXT("RevisionFilesNone", "Select a revision to see its files"));
	}
	if (RevisionMessageBox.IsValid())
	{
		RevisionMessageBox->SetText(FText::GetEmpty());
	}
}

void SArcLoreSourceControlPanel::OnHistorySelectionChanged(TSharedPtr<FArcLoreSourceControlRevision, ESPMode::ThreadSafe> InItem, ESelectInfo::Type InSelectInfo)
{
	// Resolve the live selection (the delegate's InItem can be a default-constructed
	// placeholder when the selection is cleared, so read the view directly).
	TArray<TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>> Selected;
	if (HistoryListView.IsValid())
	{
		Selected = HistoryListView->GetSelectedItems();
	}

	// Selection cleared — empty the sub-list, reset the caption, clear the message box.
	if (Selected.Num() == 0)
	{
		RevisionFileItems.Reset();
		if (RevisionFilesView.IsValid())
		{
			RevisionFilesView->RequestListRefresh();
		}
		if (RevisionFilesHeader.IsValid())
		{
			RevisionFilesHeader->SetText(LOCTEXT("RevisionFilesNone", "Select a revision to see its files"));
		}
		if (RevisionMessageBox.IsValid())
		{
			RevisionMessageBox->SetText(FText::GetEmpty());
		}
		return;
	}

	const TSharedRef<FArcLoreSourceControlRevision, ESPMode::ThreadSafe>& Revision = Selected[0];

	if (RevisionFilesHeader.IsValid())
	{
		RevisionFilesHeader->SetText(FText::Format(
			LOCTEXT("RevisionFilesCaption", "Files in revision #{0}"),
			FText::AsNumber(Revision->GetRevisionNumber())));
	}

	// Show the full commit message of the selected revision.
	if (RevisionMessageBox.IsValid())
	{
		RevisionMessageBox->SetText(FText::FromString(Revision->GetDescription()));
	}

	// Issue the async op to list the files changed in the selected revision. Bind the
	// op into the completion delegate so it stays alive and its Files payload is
	// readable on completion.
	FArcLoreSourceControlProvider& Provider = GetProvider();
	TSharedRef<FLoreRevisionFiles> Op = ISourceControlOperation::Create<FLoreRevisionFiles>();
	Op->Revision = Revision->RevisionHashHex;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnRevisionFilesComplete, Op));
}

void SArcLoreSourceControlPanel::OnRevisionFilesComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRevisionFiles> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	const FString RepoRoot = GetProvider().GetPathToRepositoryRoot();
	RevisionFileItems.Reset();
	RevisionFileItems.Reserve(InOp->Files.Num());
	for (const FArcLoreUnpushedFile& File : InOp->Files)
	{
		TSharedPtr<FArcLoreUnpushedFile> NewItem = MakeShared<FArcLoreUnpushedFile>(File);
		NewItem->AbsPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(RepoRoot, File.Path));
		RevisionFileItems.Add(NewItem);
	}

	// Sort the revision file list by the active key/direction and refresh
	// (SortRevisionFiles refreshes — replaces the inline RequestListRefresh).
	SortRevisionFiles();
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateRevisionFileRow(TSharedPtr<FArcLoreUnpushedFile> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreOutgoingRow, OwnerTable).Item(InItem);
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateBranchRow(TSharedPtr<FArcLoreBranchEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreBranchRow, OwnerTable).Item(InItem);
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateInstanceRow(TSharedPtr<FArcLoreInstanceEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreInstanceRow, OwnerTable).Item(InItem);
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateRemoteRepoRow(TSharedPtr<FArcLoreRepoEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	return SNew(SArcLoreRepoRow, OwnerTable).Item(InItem);
}

void SArcLoreSourceControlPanel::RefreshRepositories()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	// LOCAL-only op (branches + instances): safe to run on the normal Refresh path.
	// Bind the op into the completion delegate so it stays alive and its payload is
	// readable on completion.
	TSharedRef<FLoreRepositoryView> Op = ISourceControlOperation::Create<FLoreRepositoryView>();
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnRepositoryViewComplete, Op));
}

void SArcLoreSourceControlPanel::OnRepositoryViewComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreRepositoryView> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	BranchItems.Reset();
	BranchItems.Reserve(InOp->Branches.Num());
	for (const FArcLoreBranchEntry& Branch : InOp->Branches)
	{
		BranchItems.Add(MakeShared<FArcLoreBranchEntry>(Branch));
	}

	InstanceItems.Reset();
	InstanceItems.Reserve(InOp->Instances.Num());
	for (const FArcLoreInstanceEntry& Instance : InOp->Instances)
	{
		InstanceItems.Add(MakeShared<FArcLoreInstanceEntry>(Instance));
	}

	if (BranchListView.IsValid())
	{
		BranchListView->RequestListRefresh();
	}
	if (InstanceListView.IsValid())
	{
		InstanceListView->RequestListRefresh();
	}
}

FReply SArcLoreSourceControlPanel::OnListRemoteReposClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	// REMOTE listing blocks on the network with no cancel — only ever dispatched here,
	// behind this explicit button. Latch the toolbar while it runs.
	bOperationInProgress = true;

	TSharedRef<FLoreListRemoteRepos> Op = ISourceControlOperation::Create<FLoreListRemoteRepos>();
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnRemoteReposComplete, Op));

	return FReply::Handled();
}

void SArcLoreSourceControlPanel::OnRemoteReposComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreListRemoteRepos> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	RemoteRepoItems.Reset();
	RemoteRepoItems.Reserve(InOp->RemoteRepos.Num());
	for (const FArcLoreRepoEntry& Repo : InOp->RemoteRepos)
	{
		RemoteRepoItems.Add(MakeShared<FArcLoreRepoEntry>(Repo));
	}

	if (RemoteRepoListView.IsValid())
	{
		RemoteRepoListView->RequestListRefresh();
	}

	// Release the latch set in OnListRemoteReposClicked.
	bOperationInProgress = false;
}

void SArcLoreSourceControlPanel::RefreshLocks()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	// No-file op; bind the op into the completion delegate so it stays alive and its
	// Locks payload is readable on completion. Auth-gated server-side: when not logged
	// in the worker returns an empty list (a valid result).
	TSharedRef<FLoreListLocks> Op = ISourceControlOperation::Create<FLoreListLocks>();
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnLocksComplete, Op));
}

void SArcLoreSourceControlPanel::OnLocksComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreListLocks> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	LockItems.Reset();
	LockItems.Reserve(InOp->Locks.Num());
	for (const FArcLoreLockEntry& Lock : InOp->Locks)
	{
		LockItems.Add(MakeShared<FArcLoreLockEntry>(Lock));
	}

	if (LockListView.IsValid())
	{
		LockListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SArcLoreSourceControlPanel::OnGenerateLockRow(TSharedPtr<FArcLoreLockEntry> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace ArcLorePanel_Internal;
	// Pass the provider's resolved user id so the row can show "me" for the current user.
	return SNew(SArcLoreLockRow, OwnerTable)
		.Item(InItem)
		.MyUserId(GetProvider().GetUserId());
}

bool SArcLoreSourceControlPanel::CanReleaseSelectedLock() const
{
	return IsIdle() && LockListView.IsValid() && LockListView->GetNumItemsSelected() > 0;
}

FReply SArcLoreSourceControlPanel::OnReleaseSelectedLockClicked()
{
	if (!LockListView.IsValid())
	{
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FArcLoreLockEntry>> Selected = LockListView->GetSelectedItems();
	if (Selected.Num() == 0 || !Selected[0].IsValid())
	{
		return FReply::Handled();
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();

	// The lock query reports a repo-relative path; FLoreReleaseLock (lore_file_release)
	// resolves paths against the process working dir, so feed it the ABSOLUTE path
	// (combine the repo root with the repo-relative lock path).
	const FString AbsPath = FPaths::Combine(Provider.GetPathToRepositoryRoot(), Selected[0]->Path);

	bOperationInProgress = true;
	Provider.Execute(
		ISourceControlOperation::Create<FLoreReleaseLock>(),
		nullptr,
		TArray<FString>{ AbsPath },
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

FText SArcLoreSourceControlPanel::GetHeaderText() const
{
	const FArcLoreSourceControlProvider& Provider = GetProvider();

	FString Status;
	Status += FString::Printf(TEXT("Branch: %s\n"), Provider.GetBranchName().IsEmpty() ? TEXT("(unknown)") : *Provider.GetBranchName());
	Status += FString::Printf(TEXT("Remote: %s\n"), Provider.GetRemoteUrl().IsEmpty() ? TEXT("none") : *Provider.GetRemoteUrl());
	Status += FString::Printf(TEXT("Identity: %s\n"), *Provider.GetUserName());
	Status += FString::Printf(TEXT("Repository: %s"), *Provider.GetPathToRepositoryRoot());

	return FText::FromString(Status);
}

FText SArcLoreSourceControlPanel::GetUnpushedSummaryText() const
{
	const FArcLoreSourceControlProvider& Provider = GetProvider();

	if (!Provider.IsLocalAhead())
	{
		return LOCTEXT("NothingToPush", "Nothing to push");
	}

	const TOptional<int> NumUnpushed = Provider.GetCachedNumUnpushedCommits();
	if (NumUnpushed.IsSet())
	{
		return FText::Format(LOCTEXT("LocalCommitsToPushCount", "Local commits to push ({0})"), FText::AsNumber(NumUnpushed.GetValue()));
	}

	return LOCTEXT("LocalCommitsToPush", "Local commits to push");
}

FReply SArcLoreSourceControlPanel::OnTabClicked(int32 TabIndex)
{
	// Imperative switch — guarantees the body repaints (no .WidgetIndex attribute binding).
	ActiveTabIndex = TabIndex;
	if (TabSwitcher.IsValid())
	{
		TabSwitcher->SetActiveWidgetIndex(TabIndex);
	}
	// Re-list locks when the Locks tab is activated so it reflects the latest server state.
	if (TabIndex == 3)
	{
		RefreshLocks();
	}
	return FReply::Handled();
}

FSlateColor SArcLoreSourceControlPanel::GetTabButtonColor(int32 TabIndex) const
{
	// Active tab gets the full button tint; inactive tabs are dimmed.
	return TabIndex == ActiveTabIndex
		? FSlateColor(FLinearColor::White)
		: FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f));
}

bool SArcLoreSourceControlPanel::IsIdle() const
{
	return !bOperationInProgress;
}

bool SArcLoreSourceControlPanel::CanStage() const
{
	return IsIdle()
		&& ((DirtyListView.IsValid() && DirtyListView->GetNumItemsSelected() > 0)
			|| (UncontrolledListView.IsValid() && UncontrolledListView->GetNumItemsSelected() > 0));
}

bool SArcLoreSourceControlPanel::CanUnstage() const
{
	return IsIdle() && StagedListView.IsValid() && StagedListView->GetNumItemsSelected() > 0;
}

bool SArcLoreSourceControlPanel::CanRevertSelected() const
{
	return IsIdle() && DirtyListView.IsValid() && DirtyListView->GetNumItemsSelected() > 0;
}

bool SArcLoreSourceControlPanel::CanResolveSelected() const
{
	if (!IsIdle() || !DirtyListView.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FArcLorePanelFileItem>> Selected = DirtyListView->GetSelectedItems();
	for (const TSharedPtr<FArcLorePanelFileItem>& Item : Selected)
	{
		if (Item.IsValid() && Item->State->IsConflicted())
		{
			return true;
		}
	}
	return false;
}

bool SArcLoreSourceControlPanel::CanCommit() const
{
	// Summary is required; Description is optional.
	return IsIdle() && StagedItems.Num() > 0 && !CommitSummary.IsEmptyOrWhitespace();
}

bool SArcLoreSourceControlPanel::CanPush() const
{
	return IsIdle() && GetProvider().IsLocalAhead();
}

FText SArcLoreSourceControlPanel::BuildCommitMessage() const
{
	// Git convention: the first line is the subject (Summary). An optional body
	// (Description) follows after a blank line.
	if (CommitDescription.IsEmptyOrWhitespace())
	{
		return CommitSummary;
	}

	return FText::FromString(CommitSummary.ToString() + TEXT("\n\n") + CommitDescription.ToString());
}

TArray<FString> SArcLoreSourceControlPanel::GetSelectedFilenames(const TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>>& InListView) const
{
	TArray<FString> Filenames;
	if (!InListView.IsValid())
	{
		return Filenames;
	}

	TArray<TSharedPtr<FArcLorePanelFileItem>> Selected;
	InListView->GetSelectedItems(Selected);
	Filenames.Reserve(Selected.Num());
	for (const TSharedPtr<FArcLorePanelFileItem>& Item : Selected)
	{
		if (Item.IsValid())
		{
			Filenames.Add(Item->State->GetFilename());
		}
	}
	return Filenames;
}

TArray<FString> SArcLoreSourceControlPanel::GetAllStagedFilenames() const
{
	TArray<FString> Filenames;
	Filenames.Reserve(StagedItems.Num());
	for (const TSharedPtr<FArcLorePanelFileItem>& Item : StagedItems)
	{
		if (Item.IsValid())
		{
			Filenames.Add(Item->State->GetFilename());
		}
	}
	return Filenames;
}

FReply SArcLoreSourceControlPanel::OnRefreshClicked()
{
	// Repo-wide status discovery + Outgoing refresh (shared with the auto-refresh on tab open).
	RefreshAllRepoWide();

	// Repopulate the LOCAL repository view (branches/instances) on the same Refresh.
	// The REMOTE repo listing is NOT refreshed here — it is button-only.
	// FourSection has no Repositories list, so skip its remote round-trip there.
	if (LayoutMode != EArcLoreChangesLayout::FourSection)
	{
		RefreshRepositories();
	}

	return FReply::Handled();
}

FReply SArcLoreSourceControlPanel::OnStageClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	// Stage the union of selected Dirty files and (FourSection mode) selected Uncontrolled
	// files. MarkForAdd handles both modified-tracked and brand-new untracked files.
	TArray<FString> Files = GetSelectedFilenames(DirtyListView);
	if (UncontrolledListView.IsValid())
	{
		Files.Append(GetSelectedFilenames(UncontrolledListView));
	}
	if (Files.Num() == 0)
	{
		return FReply::Handled();
	}

	bOperationInProgress = true;
	Provider.Execute(
		ISourceControlOperation::Create<FMarkForAdd>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

FReply SArcLoreSourceControlPanel::OnUnstageClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	const TArray<FString> Files = GetSelectedFilenames(StagedListView);
	if (Files.Num() == 0)
	{
		return FReply::Handled();
	}

	bOperationInProgress = true;
	Provider.Execute(
		ISourceControlOperation::Create<FLoreUnstage>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

FReply SArcLoreSourceControlPanel::OnCleanStaleStagedClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();
	const TArray<FSourceControlStateRef> Stale = Provider.GetCachedStateByPredicate(
		[](const FSourceControlStateRef& S) -> bool
		{
			const FArcLoreSourceControlState& Arc =
				static_cast<const FArcLoreSourceControlState&>(S.Get());
			// ONLY staged-Added-and-gone. A staged Deleted-and-gone is a real pending
			// delete — unstaging it would silently drop the recorded deletion.
			return Arc.bStaged
				&& Arc.WorkingCopyState == EArcLoreState::Added
				&& !FPaths::FileExists(S->GetFilename());
		});
	if (Stale.Num() == 0)
	{
		return FReply::Handled();
	}
	TArray<FString> Files;
	Files.Reserve(Stale.Num());
	for (const FSourceControlStateRef& S : Stale) { Files.Add(S->GetFilename()); }

	bOperationInProgress = true;
	TWeakPtr<SArcLoreSourceControlPanel> WeakSelf = SharedThis(this);
	Provider.Execute(
		ISourceControlOperation::Create<FLoreUnstage>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateLambda(
			[WeakSelf, Files](const FSourceControlOperationRef&, ECommandResult::Type Result)
			{
				TSharedPtr<SArcLoreSourceControlPanel> Self = WeakSelf.Pin();
				if (!Self.IsValid()) { return; }
				Self->bOperationInProgress = false;
				if (Result == ECommandResult::Succeeded)
				{
					FArcLoreSourceControlProvider& P = Self->GetProvider();
					for (const FString& F : Files) { P.RemoveFileFromCache(F); }
				}
				Self->RefreshFileLists();
			}));
	return FReply::Handled();
}

FReply SArcLoreSourceControlPanel::OnSyncClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	bOperationInProgress = true;
	// FSync is a no-file/whole-checkout op: supply the repo root as the single entry so
	// the worker knows which checkout to sync (FArcLoreSyncWorker: lore_revision_sync).
	const TArray<FString> Files = { Provider.GetPathToRepositoryRoot() };
	Provider.Execute(
		ISourceControlOperation::Create<FSync>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

void SArcLoreSourceControlPanel::DoRevertSelected()
{
	const EAppReturnType::Type Choice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		LOCTEXT("RevertConfirm", "Discard local changes to the selected file(s)? This cannot be undone."));
	if (Choice != EAppReturnType::Yes)
	{
		return;
	}

	const TArray<FString> Files = GetSelectedFilenames(DirtyListView);
	if (Files.Num() == 0)
	{
		return;
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		ISourceControlOperation::Create<FRevert>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));
}

FReply SArcLoreSourceControlPanel::OnRevertClicked()
{
	DoRevertSelected();
	return FReply::Handled();
}

void SArcLoreSourceControlPanel::OnRevertSelectedFromMenu()
{
	DoRevertSelected();
}

void SArcLoreSourceControlPanel::OnResolveSelectedFromMenu()
{
	if (!DirtyListView.IsValid())
	{
		return;
	}

	const TArray<FString> Files = GetSelectedFilenames(DirtyListView);
	if (Files.Num() == 0)
	{
		return;
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		ISourceControlOperation::Create<FResolve>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));
}

TSharedPtr<SWidget> SArcLoreSourceControlPanel::OnDirtyContextMenuOpening()
{
	if (!DirtyListView.IsValid() || DirtyListView->GetNumItemsSelected() == 0)
	{
		return TSharedPtr<SWidget>();
	}

	FMenuBuilder MenuBuilder(true, nullptr);

	{
		const TArray<FString> Selected = GetSelectedFilenames(DirtyListView);
		AddOpenBrowseEntries(MenuBuilder, Selected.Num() > 0 ? Selected[0] : FString());
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ContextRevert", "Revert"),
		LOCTEXT("ContextRevertTip", "Discard local changes to the selected file(s). This cannot be undone."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::OnRevertSelectedFromMenu),
			FCanExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::CanRevertSelected)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ContextResolve", "Mark Resolved"),
		LOCTEXT("ContextResolveTip", "Mark the selected conflicted file(s) as resolved."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::OnResolveSelectedFromMenu),
			FCanExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::CanResolveSelected)));

	AddLockMenuEntries(MenuBuilder, DirtyListView);

	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SArcLoreSourceControlPanel::OnStagedContextMenuOpening()
{
	if (!StagedListView.IsValid() || StagedListView->GetNumItemsSelected() == 0)
	{
		return TSharedPtr<SWidget>();
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	{
		const TArray<FString> Selected = GetSelectedFilenames(StagedListView);
		AddOpenBrowseEntries(MenuBuilder, Selected.Num() > 0 ? Selected[0] : FString());
	}
	AddLockMenuEntries(MenuBuilder, StagedListView);
	return MenuBuilder.MakeWidget();
}

TSharedPtr<SWidget> SArcLoreSourceControlPanel::OnUncontrolledContextMenuOpening()
{
	if (!UncontrolledListView.IsValid() || UncontrolledListView->GetNumItemsSelected() == 0)
	{
		return TSharedPtr<SWidget>();
	}

	FMenuBuilder MenuBuilder(true, nullptr);

	{
		const TArray<FString> Selected = GetSelectedFilenames(UncontrolledListView);
		AddOpenBrowseEntries(MenuBuilder, Selected.Num() > 0 ? Selected[0] : FString());
	}

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ContextStage", "Stage"),
		LOCTEXT("ContextStageTip", "Add the selected uncontrolled file(s) to Lore (MarkForAdd)."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]() { OnStageClicked(); }),
			FCanExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::CanStage)));

	AddLockMenuEntries(MenuBuilder, UncontrolledListView);

	return MenuBuilder.MakeWidget();
}

void SArcLoreSourceControlPanel::AddLockMenuEntries(FMenuBuilder& MenuBuilder, TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ContextLock", "Lock"),
		LOCTEXT("ContextLockTip", "Acquire a Lore lock on the selected file(s) (check out)."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::OnLockSelected, ListView),
			FCanExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::CanLockSelected, ListView)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ContextUnlock", "Unlock"),
		LOCTEXT("ContextUnlockTip", "Release the Lore lock on the selected file(s) you hold."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::OnUnlockSelected, ListView),
			FCanExecuteAction::CreateSP(this, &SArcLoreSourceControlPanel::CanUnlockSelected, ListView)));
}

void SArcLoreSourceControlPanel::AddOpenBrowseEntries(FMenuBuilder& MenuBuilder, const FString& AbsPath)
{
	if (AbsPath.IsEmpty())
	{
		return;
	}

	const ArcLoreAssetDisplay::EArcLoreFileKind Kind = ArcLoreAssetDisplay::ClassifyFile(AbsPath);
	if (Kind == ArcLoreAssetDisplay::EArcLoreFileKind::NonAsset)
	{
		// Non-asset files (config / source / text) have no editor to open or browser target.
		return;
	}

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

TSharedPtr<SWidget> SArcLoreSourceControlPanel::OnOutgoingContextMenuOpening()
{
	if (!OutgoingTreeView.IsValid())
	{
		return TSharedPtr<SWidget>();
	}

	const TArray<TSharedPtr<FArcLoreOutgoingNode>> Selected = OutgoingTreeView->GetSelectedItems();
	if (Selected.Num() == 0)
	{
		return TSharedPtr<SWidget>();
	}

	// Only FILE nodes offer Open/Browse; commit-header nodes do not.
	const TSharedPtr<FArcLoreOutgoingNode>& Node = Selected[0];
	if (!Node.IsValid() || !Node->IsFile() || !Node->File.IsValid())
	{
		return TSharedPtr<SWidget>();
	}

	const FString AbsPath = Node->File->AbsPath.IsEmpty() ? Node->File->Path : Node->File->AbsPath;

	FMenuBuilder MenuBuilder(true, nullptr);
	AddOpenBrowseEntries(MenuBuilder, AbsPath);
	return MenuBuilder.MakeWidget();
}

void SArcLoreSourceControlPanel::OnLockSelected(TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView)
{
	const TArray<FString> Files = GetSelectedFilenames(ListView);
	if (Files.Num() == 0)
	{
		return;
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	// Lock reuses the existing FCheckOut op (lore_lock_file_acquire). A path locked by
	// ANOTHER user makes the worker fail; the provider logs the error and OnOperationComplete
	// surfaces a notification — we also pop an explicit dialog there.
	Provider.Execute(
		ISourceControlOperation::Create<FCheckOut>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));
}

void SArcLoreSourceControlPanel::OnUnlockSelected(TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView)
{
	const TArray<FString> Files = GetSelectedFilenames(ListView);
	if (Files.Num() == 0)
	{
		return;
	}

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	// Unlock uses the standalone FLoreReleaseLock op (lore_lock_file_release) — an
	// explicit release without pushing.
	Provider.Execute(
		ISourceControlOperation::Create<FLoreReleaseLock>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));
}

bool SArcLoreSourceControlPanel::CanLockSelected(TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView) const
{
	if (!IsIdle() || !ListView.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FArcLorePanelFileItem>> Selected = ListView->GetSelectedItems();
	for (const TSharedPtr<FArcLorePanelFileItem>& Item : Selected)
	{
		// At least one selected item is not already locked (lockable).
		if (Item.IsValid() && static_cast<const FArcLoreSourceControlState&>(Item->State.Get()).CanLock())
		{
			return true;
		}
	}
	return false;
}

bool SArcLoreSourceControlPanel::CanUnlockSelected(TSharedPtr<SListView<TSharedPtr<FArcLorePanelFileItem>>> ListView) const
{
	if (!IsIdle() || !ListView.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FArcLorePanelFileItem>> Selected = ListView->GetSelectedItems();
	for (const TSharedPtr<FArcLorePanelFileItem>& Item : Selected)
	{
		// At least one selected item is locked by me (releasable).
		if (Item.IsValid() && static_cast<const FArcLoreSourceControlState&>(Item->State.Get()).IsLockedByMe())
		{
			return true;
		}
	}
	return false;
}

FReply SArcLoreSourceControlPanel::OnCommitClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	const TArray<FString> Files = GetAllStagedFilenames();
	if (Files.Num() == 0 || CommitSummary.IsEmptyOrWhitespace())
	{
		return FReply::Handled();
	}

	TSharedRef<FCheckIn> CheckInOp = ISourceControlOperation::Create<FCheckIn>();
	CheckInOp->SetDescription(BuildCommitMessage());

	bOperationInProgress = true;
	Provider.Execute(
		CheckInOp,
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	// Clear the summary + description after dispatch.
	CommitSummary = FText::GetEmpty();
	CommitDescription = FText::GetEmpty();
	if (CommitSummaryBox.IsValid())
	{
		CommitSummaryBox->SetText(FText::GetEmpty());
	}
	if (CommitDescriptionBox.IsValid())
	{
		CommitDescriptionBox->SetText(FText::GetEmpty());
	}

	return FReply::Handled();
}

FReply SArcLoreSourceControlPanel::OnCommitAndPushClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	const TArray<FString> Files = GetAllStagedFilenames();
	if (Files.Num() == 0 || CommitSummary.IsEmptyOrWhitespace())
	{
		return FReply::Handled();
	}

	TSharedRef<FCheckIn> CheckInOp = ISourceControlOperation::Create<FCheckIn>();
	CheckInOp->SetDescription(BuildCommitMessage());

	bOperationInProgress = true;
	// Carry the committed file set into the completion delegate so the chained Push can
	// release their locks (Step 2 worker releases locks only for files it is supplied).
	Provider.Execute(
		CheckInOp,
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnCommitThenPushComplete, Files));

	// Clear the summary + description after dispatch.
	CommitSummary = FText::GetEmpty();
	CommitDescription = FText::GetEmpty();
	if (CommitSummaryBox.IsValid())
	{
		CommitSummaryBox->SetText(FText::GetEmpty());
	}
	if (CommitDescriptionBox.IsValid())
	{
		CommitDescriptionBox->SetText(FText::GetEmpty());
	}

	return FReply::Handled();
}

void SArcLoreSourceControlPanel::OnCommitThenPushComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TArray<FString> CommittedFiles)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate/provider.
	if (InResult != ECommandResult::Succeeded)
	{
		// Commit failed: do NOT push. Behave like OnOperationComplete (clear flag + refresh).
		UE_LOGF(LogSourceControl, Warning,"ArcLore: Commit & Push — commit failed; skipping push.");
		bOperationInProgress = false;
		RefreshFileLists();
		return;
	}

	// Keep bOperationInProgress set through the push (do NOT clear between commit and push).
	UE_LOGF(LogSourceControl, Verbose,"ArcLore: Commit & Push — commit succeeded; pushing %d file(s) to release locks.", CommittedFiles.Num());

	FArcLoreSourceControlProvider& Provider = GetProvider();
	Provider.Execute(
		ISourceControlOperation::Create<FLorePush>(),
		nullptr,
		CommittedFiles,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));
}

FReply SArcLoreSourceControlPanel::OnPushClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	// Supply the staged/dirty file set so the Push worker releases their locks (Step 2
	// releases locks only when files are supplied). v1: use the current staged set, or
	// fall back to the dirty set when nothing is staged.
	TArray<FString> Files = GetAllStagedFilenames();
	if (Files.Num() == 0)
	{
		Files = GetSelectedFilenames(DirtyListView);
	}

	bOperationInProgress = true;
	Provider.Execute(
		ISourceControlOperation::Create<FLorePush>(),
		nullptr,
		Files,
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

FText SArcLoreSourceControlPanel::GetIdentityText() const
{
	if (bIsLoggingIn)
	{
		return LOCTEXT("ArcLore_SigningIn", "Signing in… (complete sign-in in your browser)");
	}

	const FArcLoreSourceControlProvider& Provider = GetProvider();

	if (Provider.IsAuthResolved() && !Provider.GetUserName().IsEmpty())
	{
		return FText::Format(LOCTEXT("IdentitySignedIn", "Signed in: {0}"), FText::FromString(Provider.GetUserName()));
	}

	return LOCTEXT("IdentityNotLoggedIn", "Not logged in");
}

bool SArcLoreSourceControlPanel::CanLogin() const
{
	return IsIdle() && !GetProvider().IsAuthResolved();
}

bool SArcLoreSourceControlPanel::CanLogout() const
{
	return IsIdle() && GetProvider().IsAuthResolved();
}

FReply SArcLoreSourceControlPanel::OnLoginInteractiveClicked()
{
	TSharedRef<FLoreLoginInteractive> Op = ISourceControlOperation::Create<FLoreLoginInteractive>();

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bIsLoggingIn = true;
	bOperationInProgress = true;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnInteractiveLoginComplete, Op));

	return FReply::Handled();
}

void SArcLoreSourceControlPanel::OnInteractiveLoginComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreLoginInteractive> InOp)
{
	// Clear on every path — success, failure, and timeout.
	bIsLoggingIn = false;
	bOperationInProgress = false;

	if (!InOp->bSuccess)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("InteractiveLoginFailed", "Sign-in failed — the browser login did not complete successfully."));
	}

	// Header identity text re-evaluates on next paint via the bound Text_SP delegate;
	// no explicit invalidation needed. On success, UpdateStates() called SetAuthResolved(true)
	// and broadcast OnSourceControlStateChanged, which already updated the provider state.
}

FReply SArcLoreSourceControlPanel::OnLoginClicked()
{
	// Modal token-entry dialog hosted in an SWindow.
	// We capture the entered token and type via shared pointers that the OK handler reads.
	TSharedPtr<SEditableTextBox> TokenBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> TypeCombo;

	// The window is created on the stack and managed by Slate (AddModalWindow keeps a ref).
	TSharedRef<SWindow> LoginWindow = SNew(SWindow)
		.Title(LOCTEXT("LoginWindowTitle", "Login with Token"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	// Capture the window ptr so the OK/Cancel lambdas can close it.
	TSharedPtr<SWindow> LoginWindowPtr = LoginWindow;

	// Shared flag: true when the user confirmed (pressed OK).
	TSharedPtr<bool> bConfirmed = MakeShared<bool>(false);

	LoginWindow->SetContent(
		SNew(SBox)
		.MinDesiredWidth(360.0f)
		.Padding(FMargin(16.0f))
		[
			SNew(SVerticalBox)

			// Token field
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TokenLabel", "Token"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SAssignNew(TokenBox, SEditableTextBox)
				.IsPassword(true)
				.HintText(LOCTEXT("TokenHint", "Paste token here..."))
			]

			// Token-type combo
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TokenTypeLabel", "Token type"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SAssignNew(TypeCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&TokenTypeOptions)
				.InitiallySelectedItem(SelectedTokenType)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> InValue, ESelectInfo::Type) -> void
				{
					if (InValue.IsValid())
					{
						SelectedTokenType = InValue;
					}
				})
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> InItem) -> TSharedRef<SWidget>
				{
					return SNew(STextBlock)
						.Text(InItem.IsValid() ? FText::FromString(*InItem) : FText::GetEmpty());
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]() -> FText
					{
						return SelectedTokenType.IsValid()
							? FText::FromString(*SelectedTokenType)
							: FText::GetEmpty();
					})
				]
			]

			// OK / Cancel row
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("LoginOK", "OK"))
					.OnClicked_Lambda([LoginWindowPtr, bConfirmed]() -> FReply
					{
						*bConfirmed = true;
						if (LoginWindowPtr.IsValid())
						{
							LoginWindowPtr->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("LoginCancel", "Cancel"))
					.OnClicked_Lambda([LoginWindowPtr]() -> FReply
					{
						if (LoginWindowPtr.IsValid())
						{
							LoginWindowPtr->RequestDestroyWindow();
						}
						return FReply::Handled();
					})
				]
			]
		]
	);

	// Block until the window is dismissed (modal).
	FSlateApplication::Get().AddModalWindow(LoginWindow, FSlateApplication::Get().GetActiveTopLevelWindow());

	// Window has been closed — read results.
	if (!(*bConfirmed))
	{
		return FReply::Handled();
	}

	const FString EnteredToken = TokenBox.IsValid() ? TokenBox->GetText().ToString() : FString();
	if (EnteredToken.IsEmpty())
	{
		return FReply::Handled();
	}

	const FString EnteredType = SelectedTokenType.IsValid() ? *SelectedTokenType : TEXT("lore");

	TSharedRef<FLoreLoginWithToken> Op = ISourceControlOperation::Create<FLoreLoginWithToken>();
	Op->Token = EnteredToken;
	Op->TokenType = EnteredType;

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnLoginComplete, Op));

	return FReply::Handled();
}

void SArcLoreSourceControlPanel::OnLoginComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreLoginWithToken> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	bOperationInProgress = false;

	if (!InOp->bSuccess)
	{
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("LoginFailed", "Login failed — check the token and token type."));
	}

	// Header identity text re-evaluates on next paint via the bound Text_SP delegate;
	// no explicit invalidation needed.
}

FReply SArcLoreSourceControlPanel::OnLogoutClicked()
{
	FArcLoreSourceControlProvider& Provider = GetProvider();

	bOperationInProgress = true;
	Provider.Execute(
		ISourceControlOperation::Create<FLoreLogout>(),
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnOperationComplete));

	return FReply::Handled();
}

bool SArcLoreSourceControlPanel::CanSwitchBranch() const
{
	if (!IsIdle() || !BranchListView.IsValid() || BranchListView->GetNumItemsSelected() == 0)
	{
		return false;
	}

	const TArray<TSharedPtr<FArcLoreBranchEntry>> Selected = BranchListView->GetSelectedItems();
	if (Selected.Num() == 0 || !Selected[0].IsValid())
	{
		return false;
	}

	// Disable if the selected branch is already the current one.
	return !Selected[0]->bIsCurrent;
}

FReply SArcLoreSourceControlPanel::OnSwitchBranchClicked()
{
	if (!BranchListView.IsValid())
	{
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FArcLoreBranchEntry>> Selected = BranchListView->GetSelectedItems();
	if (Selected.Num() == 0 || !Selected[0].IsValid())
	{
		return FReply::Handled();
	}

	const FString Branch = Selected[0]->Name;

	// Use panel's own lists as a proxy for dirty state (populated from the provider's
	// state cache — semantically equivalent to CachedNumLocalChanges/CachedStagedCount).
	const bool bHasStaged = StagedItems.Num() > 0;
	const bool bHasLocal  = DirtyItems.Num() > 0;

	bool bReset = false;

	if (bHasStaged || bHasLocal)
	{
		const EAppReturnType::Type Choice = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			FText::Format(
				LOCTEXT("SwitchDirtyWarn",
					"You have uncommitted changes. Switching to '{0}' may be blocked, or with Reset will DISCARD changes to files the target branch modifies.\n\n"
					"Yes = try switch (keep changes), No = switch with RESET (discard), Cancel = abort."),
				FText::FromString(Branch)));

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
			return FReply::Handled();
		}
	}

	TSharedRef<FLoreBranchSwitch> Op = ISourceControlOperation::Create<FLoreBranchSwitch>();
	Op->Branch = Branch;
	Op->bReset = bReset;

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnBranchSwitchComplete, Op));

	return FReply::Handled();
}

void SArcLoreSourceControlPanel::OnBranchSwitchComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchSwitch> InOp)
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

	// Refresh both the repository view (branch list, current-branch marker) and file lists
	// (the working copy has changed — dirty/staged states need re-evaluation).
	RefreshRepositories();
	RefreshFileLists();
}

FReply SArcLoreSourceControlPanel::OnCreateBranchClicked()
{
	// Modal dialog with a name field and an optional category field.
	TSharedPtr<SEditableTextBox> NameBox;
	TSharedPtr<SEditableTextBox> CategoryBox;

	TSharedRef<SWindow> CreateWindow = SNew(SWindow)
		.Title(LOCTEXT("CreateBranchWindowTitle", "Create Branch"))
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	// Capture the window ptr so the OK/Cancel lambdas can close it.
	TSharedPtr<SWindow> CreateWindowPtr = CreateWindow;

	// Shared flag: true when the user confirmed (pressed OK).
	TSharedPtr<bool> bConfirmed = MakeShared<bool>(false);

	CreateWindow->SetContent(
		SNew(SBox)
		.MinDesiredWidth(340.0f)
		.Padding(FMargin(16.0f))
		[
			SNew(SVerticalBox)

			// Branch name
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
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SAssignNew(NameBox, SEditableTextBox)
				.HintText(LOCTEXT("CreateBranchNameHint", "Enter branch name..."))
			]

			// Category (optional)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CreateBranchCategoryLabel", "Category (optional)"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SAssignNew(CategoryBox, SEditableTextBox)
				.HintText(LOCTEXT("CreateBranchCategoryHint", "Category (optional)..."))
			]

			// OK / Cancel row
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

	// Window has been closed — read results.
	if (!(*bConfirmed))
	{
		return FReply::Handled();
	}

	const FString BranchName = NameBox.IsValid() ? NameBox->GetText().ToString() : FString();
	if (BranchName.IsEmpty())
	{
		return FReply::Handled();
	}

	const FString Category = CategoryBox.IsValid() ? CategoryBox->GetText().ToString() : FString();

	TSharedRef<FLoreBranchCreate> Op = ISourceControlOperation::Create<FLoreBranchCreate>();
	Op->Name     = BranchName;
	Op->Category = Category;

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		Op,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnBranchCreateComplete, Op));

	return FReply::Handled();
}

void SArcLoreSourceControlPanel::OnBranchCreateComplete(const FSourceControlOperationRef& InOperation, ECommandResult::Type InResult, TSharedRef<FLoreBranchCreate> InOp)
{
	// Provider Tick dispatches completion on the game thread — safe to touch Slate.
	bOperationInProgress = false;

	// Always refresh the branch list so the new branch (or lack thereof) is visible.
	RefreshRepositories();

	if (!InOp->bSucceeded)
	{
		// Worker already logged the error; nothing extra to surface for create failures
		// (the op has no ErrorText field, so we emit a generic notice).
		FMessageDialog::Open(
			EAppMsgType::Ok,
			LOCTEXT("CreateBranchFailed", "Branch creation failed. See the Output Log for details."));
		return;
	}

	// Offer to switch to the newly created branch.
	const EAppReturnType::Type Choice = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::Format(
			LOCTEXT("CreateBranchSwitchOffer", "Branch '{0}' created. Switch to it now?"),
			FText::FromString(InOp->Name)));

	if (Choice != EAppReturnType::Yes)
	{
		return;
	}

	// Clean switch: the branch was just created from the current revision; no local changes
	// can be blocking, so bReset=false is always correct here.
	TSharedRef<FLoreBranchSwitch> SwitchOp = ISourceControlOperation::Create<FLoreBranchSwitch>();
	SwitchOp->Branch = InOp->Name;
	SwitchOp->bReset = false;

	FArcLoreSourceControlProvider& Provider = GetProvider();
	bOperationInProgress = true;
	Provider.Execute(
		SwitchOp,
		nullptr,
		TArray<FString>(),
		EConcurrency::Asynchronous,
		FSourceControlOperationComplete::CreateSP(this, &SArcLoreSourceControlPanel::OnBranchSwitchComplete, SwitchOp));
}

#undef LOCTEXT_NAMESPACE
