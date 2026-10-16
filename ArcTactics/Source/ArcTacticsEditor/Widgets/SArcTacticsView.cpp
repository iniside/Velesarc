// Copyright Lukasz Baran. All Rights Reserved.

#include "Widgets/SArcTacticsView.h"

#include "ArcTacticsEditorCommands.h"
#include "ArcTacticsEditorViewModel.h"
#include "ArcTacticsNodePresentation.h"
#include "ArcTacticsNodeTypePicker.h"
#include "Decision/ArcTacticsConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/SArcTacticsNodeRow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "SArcTacticsView"

namespace ArcTacticsViewPrivate
{
	TSharedRef<FArcTacticsViewItem> MakeGroup(
		const EArcTacticsViewGroup Group,
		const FText& Label)
	{
		TSharedRef<FArcTacticsViewItem> Item = MakeShared<FArcTacticsViewItem>();
		Item->Group = Group;
		Item->Label = Label;
		return Item;
	}

	TSharedRef<FArcTacticsViewItem> MakeNode(
		const EArcTacticsEditorItemKind Kind,
		const FGuid& ID)
	{
		TSharedRef<FArcTacticsViewItem> Item = MakeShared<FArcTacticsViewItem>();
		Item->Selection = {Kind, ID};
		return Item;
	}

	TSharedRef<FArcTacticsViewItem> MakeActiveStepsGroup(
		const FArcTacticsEditorSelection& Owner,
		const TArray<FArcTacticsNodeDefinition>& ActiveSteps)
	{
		TSharedRef<FArcTacticsViewItem> Group = MakeGroup(
			EArcTacticsViewGroup::ActiveSteps, LOCTEXT("ActiveStepsGroup", "While Active"));
		Group->Selection = Owner;
		for (const FArcTacticsNodeDefinition& ActiveStep : ActiveSteps)
		{
			Group->Children.Add(MakeNode(EArcTacticsEditorItemKind::ActiveStep, ActiveStep.ID));
		}
		return Group;
	}

	template <typename DefinitionType>
	const TArray<FArcTacticsNodeDefinition>& GetBranchNodes(
		const DefinitionType& Definition,
		const EArcTacticsEditorItemKind Kind)
	{
		switch (Kind)
		{
		case EArcTacticsEditorItemKind::Step:
			return Definition.Steps;
		case EArcTacticsEditorItemKind::ActiveStep:
			return Definition.ActiveSteps;
		default:
			return Definition.Considerations;
		}
	}

	FText GetPlainText(
		const TWeakPtr<FArcTacticsEditorViewModel>& WeakViewModel,
		const FArcTacticsEditorSelection& Selection)
	{
		const TSharedPtr<FArcTacticsEditorViewModel> Model = WeakViewModel.Pin();
		UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
		if (!Config)
		{
			return FText::GetEmpty();
		}
		if (Selection.Kind == EArcTacticsEditorItemKind::Context)
		{
			return FArcTacticsNodePresentation::BuildContext().PlainText;
		}
		const FArcTacticsNodeDefinition* Node = Config->FindNodeByID(Selection.ID);
		return Node
			? FArcTacticsNodePresentation::Build(*Node, *Config).PlainText
			: FText::GetEmpty();
	}
}

SArcTacticsView::~SArcTacticsView()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->OnModelChanged().RemoveAll(this);
		Model->OnSelectionChanged().RemoveAll(this);
	}
}

void SArcTacticsView::Construct(
	const FArguments& InArgs,
	TSharedRef<FArcTacticsEditorViewModel> InViewModel,
	TSharedRef<FUICommandList> InCommandList)
{
	ViewModel = InViewModel;
	CommandList = InCommandList;
	InViewModel->OnModelChanged().AddSP(this, &SArcTacticsView::HandleModelChanged);
	InViewModel->OnSelectionChanged().AddSP(this, &SArcTacticsView::HandleModelSelectionChanged);
	BindCommands();

	ExpansionKeys.Add({EArcTacticsViewGroup::Context, FArcTacticsEditorSelection::Config()});
	ExpansionKeys.Add({EArcTacticsViewGroup::GlobalTasks, FArcTacticsEditorSelection::Config()});
	ExpansionKeys.Add({EArcTacticsViewGroup::Decisions, FArcTacticsEditorSelection::Config()});
	ExpansionKeys.Add({EArcTacticsViewGroup::Actions, FArcTacticsEditorSelection::Config()});
	ExpansionKeys.Add({EArcTacticsViewGroup::Selectors, FArcTacticsEditorSelection::Config()});

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f)
		[
			SAssignNew(SearchBox, SSearchBox)
			.HintText(LOCTEXT("SearchHint", "Search tasks, actions, selectors, and considerations"))
			.OnTextChanged(this, &SArcTacticsView::HandleSearchChanged)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
				.OnClicked_Lambda([this]()
				{
					ExpandAll();
					return FReply::Handled();
				})
				.ToolTipText(LOCTEXT("ExpandAllTooltip", "Expand all groups"))
				[
					SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.Plus")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
				.OnClicked_Lambda([this]()
				{
					CollapseAll();
					return FReply::Handled();
				})
				.ToolTipText(LOCTEXT("CollapseAllTooltip", "Collapse all groups"))
				[
					SNew(SImage).Image(FAppStyle::GetBrush(TEXT("Icons.Minus")))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSeparator)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(TreeView, STreeView<TSharedPtr<FArcTacticsViewItem>>)
			.TreeItemsSource(&RootItems)
			.OnGenerateRow(this, &SArcTacticsView::HandleGenerateRow)
			.OnGetChildren(this, &SArcTacticsView::HandleGetChildren)
			.OnSelectionChanged(this, &SArcTacticsView::HandleSelectionChanged)
			.OnExpansionChanged(this, &SArcTacticsView::HandleExpansionChanged)
			.OnMouseButtonDoubleClick(this, &SArcTacticsView::HandleDoubleClick)
			.OnContextMenuOpening(this, &SArcTacticsView::HandleContextMenuOpening)
			.SelectionMode(ESelectionMode::Single)
			.AllowOverscroll(EAllowOverscroll::Yes)
		]
	];

	RebuildItems(false);
}

FReply SArcTacticsView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent)
		? FReply::Handled()
		: SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SArcTacticsView::BindCommands()
{
	const FArcTacticsEditorCommands& Commands = FArcTacticsEditorCommands::Get();
	CommandList->MapAction(
		Commands.AddGlobalTask,
		FExecuteAction::CreateSP(this, &SArcTacticsView::OpenAddPicker, EArcTacticsEditorItemKind::GlobalTask));
	CommandList->MapAction(
		Commands.AddAction,
		FExecuteAction::CreateSP(this, &SArcTacticsView::OpenAddPicker, EArcTacticsEditorItemKind::Action));
	CommandList->MapAction(
		Commands.AddSelector,
		FExecuteAction::CreateSP(this, &SArcTacticsView::OpenAddPicker, EArcTacticsEditorItemKind::Selector));
	CommandList->MapAction(
		Commands.AddStep,
		FExecuteAction::CreateSP(this, &SArcTacticsView::OpenAddPicker, EArcTacticsEditorItemKind::Step),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanAddStep));
	CommandList->MapAction(
		Commands.AddActiveStep,
		FExecuteAction::CreateSP(this, &SArcTacticsView::OpenAddPicker, EArcTacticsEditorItemKind::ActiveStep),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanAddActiveStep));
	CommandList->MapAction(
		Commands.AddConsideration,
		FExecuteAction::CreateSP(this, &SArcTacticsView::OpenAddPicker, EArcTacticsEditorItemKind::Consideration),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanAddConsideration));
	CommandList->MapAction(
		Commands.Rename,
		FExecuteAction::CreateSP(this, &SArcTacticsView::RequestRename),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanRename));
	CommandList->MapAction(
		Commands.ReplaceType,
		FExecuteAction::CreateSP(this, &SArcTacticsView::OpenReplacePicker),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanReplaceSelection));
	CommandList->MapAction(
		Commands.Delete,
		FExecuteAction::CreateSP(this, &SArcTacticsView::DeleteSelection),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanDeleteSelection));
	CommandList->MapAction(
		Commands.Duplicate,
		FExecuteAction::CreateSP(this, &SArcTacticsView::DuplicateSelection),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanDuplicateSelection));
	CommandList->MapAction(
		Commands.Copy,
		FExecuteAction::CreateSP(this, &SArcTacticsView::CopySelection),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanCopySelection));
	CommandList->MapAction(
		Commands.Paste,
		FExecuteAction::CreateSP(this, &SArcTacticsView::PasteSelection),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanPasteSelection));
	CommandList->MapAction(
		Commands.MoveUp,
		FExecuteAction::CreateSP(this, &SArcTacticsView::MoveSelection, true),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanMoveSelection, true));
	CommandList->MapAction(
		Commands.MoveDown,
		FExecuteAction::CreateSP(this, &SArcTacticsView::MoveSelection, false),
		FCanExecuteAction::CreateSP(this, &SArcTacticsView::CanMoveSelection, false));
}

void SArcTacticsView::RebuildItems(const bool bPreserveExpansion)
{
	if (!TreeView.IsValid())
	{
		return;
	}
	if (bPreserveExpansion && !bFiltering)
	{
		CaptureExpansion(ExpansionKeys);
	}

	TArray<TSharedPtr<FArcTacticsViewItem>> AllItems;
	BuildItems(AllItems);
	RootItems.Reset();
	if (SearchText.IsEmpty())
	{
		RootItems = MoveTemp(AllItems);
	}
	else
	{
		const FString Filter = SearchText.ToLower();
		for (const TSharedPtr<FArcTacticsViewItem>& Item : AllItems)
		{
			if (const TSharedPtr<FArcTacticsViewItem> Filtered = FilterItem(Item, Filter))
			{
				RootItems.Add(Filtered);
			}
		}
	}

	TreeView->SetTreeItemsSource(&RootItems);
	TreeView->RequestTreeRefresh();
	if (bFiltering)
	{
		ExpandAll();
	}
	else
	{
		RestoreExpansion(ExpansionKeys);
	}
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		SynchronizeSelection(Model->GetSelection(), false);
	}
}

void SArcTacticsView::BuildItems(TArray<TSharedPtr<FArcTacticsViewItem>>& OutItems) const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	const UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
	if (!Config)
	{
		return;
	}

	const TSharedRef<FArcTacticsViewItem> Context = ArcTacticsViewPrivate::MakeGroup(
		EArcTacticsViewGroup::Context, LOCTEXT("ContextGroup", "Context"));
	Context->Children.Add(ArcTacticsViewPrivate::MakeNode(
		EArcTacticsEditorItemKind::Context, Config->ContextID));
	OutItems.Add(Context);

	const TSharedRef<FArcTacticsViewItem> GlobalTasks = ArcTacticsViewPrivate::MakeGroup(
		EArcTacticsViewGroup::GlobalTasks, LOCTEXT("GlobalTasksGroup", "Global Tasks"));
	for (const FArcTacticsNodeDefinition& Task : Config->GlobalTasks)
	{
		GlobalTasks->Children.Add(ArcTacticsViewPrivate::MakeNode(
			EArcTacticsEditorItemKind::GlobalTask, Task.ID));
	}
	OutItems.Add(GlobalTasks);

	const TSharedRef<FArcTacticsViewItem> Decisions = ArcTacticsViewPrivate::MakeGroup(
		EArcTacticsViewGroup::Decisions, LOCTEXT("DecisionsGroup", "Decisions"));
	const TSharedRef<FArcTacticsViewItem> Actions = ArcTacticsViewPrivate::MakeGroup(
		EArcTacticsViewGroup::Actions, LOCTEXT("ActionsGroup", "Actions"));
	for (const FArcTacticsActionDefinition& Action : Config->Actions)
	{
		const TSharedRef<FArcTacticsViewItem> ActionItem = ArcTacticsViewPrivate::MakeNode(
			EArcTacticsEditorItemKind::Action, Action.Action.ID);
		ActionItem->Children.Add(ArcTacticsViewPrivate::MakeActiveStepsGroup(
			ActionItem->Selection, Action.ActiveSteps));
		for (const FArcTacticsNodeDefinition& Step : Action.Steps)
		{
			ActionItem->Children.Add(ArcTacticsViewPrivate::MakeNode(
				EArcTacticsEditorItemKind::Step, Step.ID));
		}
		for (const FArcTacticsNodeDefinition& Consideration : Action.Considerations)
		{
			ActionItem->Children.Add(ArcTacticsViewPrivate::MakeNode(
				EArcTacticsEditorItemKind::Consideration, Consideration.ID));
		}
		Actions->Children.Add(ActionItem);
	}
	Decisions->Children.Add(Actions);

	const TSharedRef<FArcTacticsViewItem> Selectors = ArcTacticsViewPrivate::MakeGroup(
		EArcTacticsViewGroup::Selectors, LOCTEXT("SelectorsGroup", "Selectors"));
	for (const FArcTacticsSelectorDefinition& Selector : Config->Selectors)
	{
		const TSharedRef<FArcTacticsViewItem> SelectorItem = ArcTacticsViewPrivate::MakeNode(
			EArcTacticsEditorItemKind::Selector, Selector.Selector.ID);
		const TSharedRef<FArcTacticsViewItem> SelectorActionItem = ArcTacticsViewPrivate::MakeNode(
			EArcTacticsEditorItemKind::SelectorAction, Selector.Action.ID);
		SelectorActionItem->Children.Add(ArcTacticsViewPrivate::MakeActiveStepsGroup(
			SelectorActionItem->Selection, Selector.ActiveSteps));
		for (const FArcTacticsNodeDefinition& Step : Selector.Steps)
		{
			SelectorActionItem->Children.Add(ArcTacticsViewPrivate::MakeNode(
				EArcTacticsEditorItemKind::Step, Step.ID));
		}
		for (const FArcTacticsNodeDefinition& Consideration : Selector.Considerations)
		{
			SelectorActionItem->Children.Add(ArcTacticsViewPrivate::MakeNode(
				EArcTacticsEditorItemKind::Consideration, Consideration.ID));
		}
		SelectorItem->Children.Add(SelectorActionItem);
		Selectors->Children.Add(SelectorItem);
	}
	Decisions->Children.Add(Selectors);
	OutItems.Add(Decisions);
}

TSharedPtr<FArcTacticsViewItem> SArcTacticsView::FilterItem(
	const TSharedPtr<FArcTacticsViewItem>& Item,
	const FString& Filter) const
{
	if (!Item.IsValid())
	{
		return nullptr;
	}
	if (ItemMatchesFilter(*Item, Filter))
	{
		return Item;
	}
	TSharedPtr<FArcTacticsViewItem> Filtered = MakeShared<FArcTacticsViewItem>();
	Filtered->Group = Item->Group;
	Filtered->Selection = Item->Selection;
	Filtered->Label = Item->Label;
	for (const TSharedPtr<FArcTacticsViewItem>& Child : Item->Children)
	{
		if (const TSharedPtr<FArcTacticsViewItem> FilteredChild = FilterItem(Child, Filter))
		{
			Filtered->Children.Add(FilteredChild);
		}
	}
	return Filtered->Children.IsEmpty() ? nullptr : Filtered;
}

bool SArcTacticsView::ItemMatchesFilter(
	const FArcTacticsViewItem& Item,
	const FString& Filter) const
{
	const FText Text = Item.IsGroup()
		? Item.Label
		: ArcTacticsViewPrivate::GetPlainText(ViewModel, Item.Selection);
	return Text.ToString().ToLower().Contains(Filter);
}

FArcTacticsViewItemKey SArcTacticsView::MakeKey(const FArcTacticsViewItem& Item) const
{
	return {Item.Group, Item.Selection};
}

void SArcTacticsView::CaptureExpansion(TSet<FArcTacticsViewItemKey>& OutKeys) const
{
	OutKeys.Reset();
	TSet<TSharedPtr<FArcTacticsViewItem>> ExpandedItems;
	TreeView->GetExpandedItems(ExpandedItems);
	for (const TSharedPtr<FArcTacticsViewItem>& Item : ExpandedItems)
	{
		if (Item.IsValid())
		{
			OutKeys.Add(MakeKey(*Item));
		}
	}
}

void SArcTacticsView::RestoreExpansion(const TSet<FArcTacticsViewItemKey>& Keys)
{
	TArray<TSharedPtr<FArcTacticsViewItem>> Pending = RootItems;
	while (!Pending.IsEmpty())
	{
		const TSharedPtr<FArcTacticsViewItem> Item = Pending.Pop(EAllowShrinking::No);
		if (!Item.IsValid())
		{
			continue;
		}
		TreeView->SetItemExpansion(Item, Keys.Contains(MakeKey(*Item)));
		Pending.Append(Item->Children);
	}
}

void SArcTacticsView::ExpandAll()
{
	TArray<TSharedPtr<FArcTacticsViewItem>> Pending = RootItems;
	while (!Pending.IsEmpty())
	{
		const TSharedPtr<FArcTacticsViewItem> Item = Pending.Pop(EAllowShrinking::No);
		if (Item.IsValid() && !Item->Children.IsEmpty())
		{
			TreeView->SetItemExpansion(Item, true);
			Pending.Append(Item->Children);
		}
	}
}

void SArcTacticsView::CollapseAll()
{
	TArray<TSharedPtr<FArcTacticsViewItem>> Pending = RootItems;
	while (!Pending.IsEmpty())
	{
		const TSharedPtr<FArcTacticsViewItem> Item = Pending.Pop(EAllowShrinking::No);
		if (Item.IsValid())
		{
			TreeView->SetItemExpansion(Item, false);
			Pending.Append(Item->Children);
		}
	}
}

TSharedRef<ITableRow> SArcTacticsView::HandleGenerateRow(
	TSharedPtr<FArcTacticsViewItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const FOnGetContent TypePicker = Item.IsValid() && !Item->IsGroup()
		&& Item->Selection.Kind != EArcTacticsEditorItemKind::Context
		? FOnGetContent::CreateSP(this, &SArcTacticsView::CreateReplaceTypePicker, Item->Selection)
		: FOnGetContent();
	return SNew(SArcTacticsNodeRow, OwnerTable, Item, ViewModel)
		.OnGetTypePicker(TypePicker);
}

void SArcTacticsView::HandleGetChildren(
	TSharedPtr<FArcTacticsViewItem> Item,
	TArray<TSharedPtr<FArcTacticsViewItem>>& OutChildren) const
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SArcTacticsView::HandleSelectionChanged(
	TSharedPtr<FArcTacticsViewItem> Item,
	ESelectInfo::Type SelectInfo)
{
	if (bSynchronizingSelection)
	{
		return;
	}
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		bSynchronizingSelection = true;
		Model->SetSelection(Item.IsValid() && !Item->IsGroup()
			? Item->Selection
			: FArcTacticsEditorSelection::Config());
		bSynchronizingSelection = false;
	}
}

void SArcTacticsView::HandleExpansionChanged(
	TSharedPtr<FArcTacticsViewItem> Item,
	const bool bExpanded)
{
	if (!bFiltering && Item.IsValid())
	{
		if (bExpanded)
		{
			ExpansionKeys.Add(MakeKey(*Item));
		}
		else
		{
			ExpansionKeys.Remove(MakeKey(*Item));
		}
	}
}

void SArcTacticsView::HandleDoubleClick(TSharedPtr<FArcTacticsViewItem> Item)
{
	if (!Item.IsValid())
	{
		return;
	}
	if (Item->IsGroup())
	{
		TreeView->SetItemExpansion(Item, !TreeView->IsItemExpanded(Item));
	}
	else
	{
		RequestRename();
	}
}

TSharedPtr<SWidget> SArcTacticsView::HandleContextMenuOpening()
{
	if (!CommandList.IsValid())
	{
		return nullptr;
	}
	FMenuBuilder MenuBuilder(true, CommandList);
	const TArray<TSharedPtr<FArcTacticsViewItem>> SelectedItems = TreeView->GetSelectedItems();
	const TSharedPtr<FArcTacticsViewItem> Selected = SelectedItems.IsEmpty()
		? nullptr
		: SelectedItems[0];
	if (Selected.IsValid()
		&& (Selected->Group == EArcTacticsViewGroup::Context
			|| Selected->Selection.Kind == EArcTacticsEditorItemKind::Context))
	{
		return nullptr;
	}
	const FArcTacticsEditorCommands& Commands = FArcTacticsEditorCommands::Get();

	if (!Selected.IsValid() || Selected->Group == EArcTacticsViewGroup::GlobalTasks)
	{
		MenuBuilder.AddMenuEntry(Commands.AddGlobalTask);
	}
	else if (Selected->Group == EArcTacticsViewGroup::Decisions)
	{
		MenuBuilder.AddMenuEntry(Commands.AddAction);
		MenuBuilder.AddMenuEntry(Commands.AddSelector);
	}
	else if (Selected->Group == EArcTacticsViewGroup::Actions)
	{
		MenuBuilder.AddMenuEntry(Commands.AddAction);
	}
	else if (Selected->Group == EArcTacticsViewGroup::Selectors)
	{
		MenuBuilder.AddMenuEntry(Commands.AddSelector);
	}
	else if (Selected->Group == EArcTacticsViewGroup::ActiveSteps)
	{
		MenuBuilder.AddMenuEntry(
			Commands.AddActiveStep->GetLabel(),
			Commands.AddActiveStep->GetDescription(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(
				this,
				&SArcTacticsView::OpenTypePicker,
				EArcTacticsEditorItemKind::ActiveStep,
				Selected->Selection,
				false)));
	}
	else if (!Selected->IsGroup())
	{
		if (Selected->Selection.Kind == EArcTacticsEditorItemKind::Action
			|| Selected->Selection.Kind == EArcTacticsEditorItemKind::SelectorAction)
		{
			MenuBuilder.AddMenuEntry(Commands.AddStep);
			MenuBuilder.AddMenuEntry(Commands.AddActiveStep);
		}
		if (Selected->Selection.Kind == EArcTacticsEditorItemKind::Action
			|| Selected->Selection.Kind == EArcTacticsEditorItemKind::Selector
			|| Selected->Selection.Kind == EArcTacticsEditorItemKind::SelectorAction)
		{
			MenuBuilder.AddMenuEntry(Commands.AddConsideration);
		}
		if (CanReplaceSelection())
		{
			MenuBuilder.AddMenuEntry(Commands.ReplaceType);
		}
		MenuBuilder.AddSeparator();
		MenuBuilder.AddMenuEntry(Commands.Rename);
		MenuBuilder.AddMenuEntry(Commands.Copy);
		MenuBuilder.AddMenuEntry(Commands.Paste);
		MenuBuilder.AddMenuEntry(Commands.Duplicate);
		MenuBuilder.AddMenuEntry(Commands.Delete);
		MenuBuilder.AddSeparator();
		MenuBuilder.AddMenuEntry(Commands.MoveUp);
		MenuBuilder.AddMenuEntry(Commands.MoveDown);
	}
	return MenuBuilder.MakeWidget();
}

void SArcTacticsView::HandleSearchChanged(const FText& Text)
{
	const bool bWasFiltering = bFiltering;
	const FString NewSearch = FText::TrimPrecedingAndTrailing(Text).ToString();
	const bool bWillFilter = !NewSearch.IsEmpty();
	if (!bWasFiltering && bWillFilter)
	{
		CaptureExpansion(UnfilteredExpansionKeys);
	}
	SearchText = NewSearch;
	bFiltering = bWillFilter;
	if (bWasFiltering && !bWillFilter)
	{
		ExpansionKeys = MoveTemp(UnfilteredExpansionKeys);
	}
	RebuildItems(false);
}

void SArcTacticsView::HandleModelChanged(const EArcTacticsAuthoringChange Change)
{
	RebuildItems(true);
}

void SArcTacticsView::HandleModelSelectionChanged(
	const FArcTacticsEditorSelection& Selection)
{
	if (!bSynchronizingSelection)
	{
		SynchronizeSelection(Selection, true);
	}
}

void SArcTacticsView::SynchronizeSelection(
	const FArcTacticsEditorSelection& Selection,
	const bool bScrollIntoView)
{
	if (!TreeView.IsValid())
	{
		return;
	}
	bSynchronizingSelection = true;
	TArray<TSharedPtr<FArcTacticsViewItem>> Path;
	if (FindItemPathRecursive(RootItems, Selection, Path))
	{
		for (int32 Index = 0; Index + 1 < Path.Num(); ++Index)
		{
			TreeView->SetItemExpansion(Path[Index], true);
		}
		const TSharedPtr<FArcTacticsViewItem> Item = Path.Last();
		TreeView->SetSelection(Item);
		if (bScrollIntoView)
		{
			TreeView->RequestScrollIntoView(Item);
		}
	}
	else if (!Selection.IsConfig())
	{
		TreeView->ClearSelection();
	}
	else
	{
		const TArray<TSharedPtr<FArcTacticsViewItem>> SelectedItems = TreeView->GetSelectedItems();
		if (!SelectedItems.IsEmpty() && SelectedItems[0].IsValid() && !SelectedItems[0]->IsGroup())
		{
			TreeView->ClearSelection();
		}
	}
	bSynchronizingSelection = false;
}

FArcTacticsEditorSelection SArcTacticsView::GetAddParent(
	const EArcTacticsEditorItemKind Kind) const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (!Model.IsValid())
	{
		return FArcTacticsEditorSelection::Config();
	}
	const FArcTacticsEditorSelection& Selected = Model->GetSelection();
	if (Kind == EArcTacticsEditorItemKind::GlobalTask
		&& Selected.Kind == EArcTacticsEditorItemKind::GlobalTask)
	{
		return Selected;
	}
	if (Kind == EArcTacticsEditorItemKind::Action
		&& Selected.Kind == EArcTacticsEditorItemKind::Action)
	{
		return Selected;
	}
	if (Kind == EArcTacticsEditorItemKind::Selector
		&& Selected.Kind == EArcTacticsEditorItemKind::Selector)
	{
		return Selected;
	}
	if ((Kind == EArcTacticsEditorItemKind::Step || Kind == EArcTacticsEditorItemKind::ActiveStep)
		&& (Selected.Kind == EArcTacticsEditorItemKind::Action
			|| Selected.Kind == EArcTacticsEditorItemKind::SelectorAction
			|| Selected.Kind == Kind))
	{
		return Selected;
	}
	if (Kind == EArcTacticsEditorItemKind::Consideration
		&& (Selected.Kind == EArcTacticsEditorItemKind::Action
			|| Selected.Kind == EArcTacticsEditorItemKind::Selector
			|| Selected.Kind == EArcTacticsEditorItemKind::SelectorAction
			|| Selected.Kind == EArcTacticsEditorItemKind::Consideration))
	{
		return Selected;
	}
	return FArcTacticsEditorSelection::Config();
}

TSharedRef<SWidget> SArcTacticsView::CreateReplaceTypePicker(
	const FArcTacticsEditorSelection Item)
{
	return ArcTacticsNodeTypePicker::MakeNodeTypePicker(
		ViewModel, Item.Kind, Item, true, EArcTacticsNodeTypePickerSurface::View);
}

void SArcTacticsView::OpenTypePicker(
	const EArcTacticsEditorItemKind Kind,
	const FArcTacticsEditorSelection Target,
	const bool bReplace)
{
	FSlateApplication::Get().PushMenu(
		SharedThis(this),
		FWidgetPath(),
		ArcTacticsNodeTypePicker::MakeNodeTypePicker(
			ViewModel, Kind, Target, bReplace, EArcTacticsNodeTypePickerSurface::View),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup);
}

void SArcTacticsView::RequestRename()
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (!Model.IsValid())
	{
		return;
	}
	const TSharedPtr<FArcTacticsViewItem> Item = FindItem(Model->GetSelection());
	const TSharedPtr<SArcTacticsNodeRow> Row = Item.IsValid()
		? StaticCastSharedPtr<SArcTacticsNodeRow>(TreeView->WidgetFromItem(Item))
		: nullptr;
	if (Row.IsValid())
	{
		Row->RequestRename();
	}
}

bool SArcTacticsView::CanRename() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Model->CanRename(Model->GetSelection());
}

void SArcTacticsView::DeleteSelection()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->Delete(Model->GetSelection());
	}
}

bool SArcTacticsView::CanDeleteSelection() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Model->CanDelete(Model->GetSelection());
}

void SArcTacticsView::DuplicateSelection()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->Duplicate(Model->GetSelection());
	}
}

bool SArcTacticsView::CanDuplicateSelection() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Model->CanDuplicate(Model->GetSelection());
}

void SArcTacticsView::CopySelection()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->Copy(Model->GetSelection());
	}
}

bool SArcTacticsView::CanCopySelection() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Model->CanCopy(Model->GetSelection());
}

void SArcTacticsView::PasteSelection()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->Paste(Model->GetSelection());
	}
}

bool SArcTacticsView::CanPasteSelection() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Model->CanPaste(Model->GetSelection());
}

void SArcTacticsView::MoveSelection(const bool bUp)
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	FArcTacticsEditorSelection Target;
	if (Model.IsValid() && FindMoveTarget(bUp, Target))
	{
		Model->Move(
			Model->GetSelection(),
			Target,
			bUp ? EArcTacticsEditorInsertLocation::Before : EArcTacticsEditorInsertLocation::After);
	}
}

bool SArcTacticsView::CanMoveSelection(const bool bUp) const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	FArcTacticsEditorSelection Target;
	return Model.IsValid() && FindMoveTarget(bUp, Target)
		&& Model->CanMove(
			Model->GetSelection(),
			Target,
			bUp ? EArcTacticsEditorInsertLocation::Before : EArcTacticsEditorInsertLocation::After);
}

bool SArcTacticsView::FindMoveTarget(
	const bool bUp,
	FArcTacticsEditorSelection& OutTarget) const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	const UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
	if (!Config)
	{
		return false;
	}
	const FArcTacticsEditorSelection& Selection = Model->GetSelection();
	if (Selection.Kind == EArcTacticsEditorItemKind::GlobalTask)
	{
		const int32 Index = Config->GlobalTasks.IndexOfByPredicate(
			[&Selection](const FArcTacticsNodeDefinition& Node) { return Node.ID == Selection.ID; });
		const int32 TargetIndex = Index + (bUp ? -1 : 1);
		if (Index != INDEX_NONE && Config->GlobalTasks.IsValidIndex(TargetIndex))
		{
			OutTarget = {Selection.Kind, Config->GlobalTasks[TargetIndex].ID};
			return true;
		}
	}
	else if (Selection.Kind == EArcTacticsEditorItemKind::Action)
	{
		const int32 Index = Config->Actions.IndexOfByPredicate(
			[&Selection](const FArcTacticsActionDefinition& Action)
			{
				return Action.Action.ID == Selection.ID;
			});
		const int32 TargetIndex = Index + (bUp ? -1 : 1);
		if (Index != INDEX_NONE && Config->Actions.IsValidIndex(TargetIndex))
		{
			OutTarget = {Selection.Kind, Config->Actions[TargetIndex].Action.ID};
			return true;
		}
	}
	else if (Selection.Kind == EArcTacticsEditorItemKind::Selector)
	{
		const int32 Index = Config->Selectors.IndexOfByPredicate(
			[&Selection](const FArcTacticsSelectorDefinition& Selector)
			{
				return Selector.Selector.ID == Selection.ID;
			});
		const int32 TargetIndex = Index + (bUp ? -1 : 1);
		if (Index != INDEX_NONE && Config->Selectors.IsValidIndex(TargetIndex))
		{
			OutTarget = {Selection.Kind, Config->Selectors[TargetIndex].Selector.ID};
			return true;
		}
	}
	else if (Selection.Kind == EArcTacticsEditorItemKind::Step
		|| Selection.Kind == EArcTacticsEditorItemKind::ActiveStep
		|| Selection.Kind == EArcTacticsEditorItemKind::Consideration)
	{
		for (const FArcTacticsActionDefinition& Action : Config->Actions)
		{
			const TArray<FArcTacticsNodeDefinition>& Nodes =
				ArcTacticsViewPrivate::GetBranchNodes(Action, Selection.Kind);
			const int32 Index = Nodes.IndexOfByPredicate(
				[&Selection](const FArcTacticsNodeDefinition& Node) { return Node.ID == Selection.ID; });
			const int32 TargetIndex = Index + (bUp ? -1 : 1);
			if (Index != INDEX_NONE && Nodes.IsValidIndex(TargetIndex))
			{
				OutTarget = {Selection.Kind, Nodes[TargetIndex].ID};
				return true;
			}
		}
		for (const FArcTacticsSelectorDefinition& Selector : Config->Selectors)
		{
			const TArray<FArcTacticsNodeDefinition>& Nodes =
				ArcTacticsViewPrivate::GetBranchNodes(Selector, Selection.Kind);
			const int32 Index = Nodes.IndexOfByPredicate(
				[&Selection](const FArcTacticsNodeDefinition& Node) { return Node.ID == Selection.ID; });
			const int32 TargetIndex = Index + (bUp ? -1 : 1);
			if (Index != INDEX_NONE && Nodes.IsValidIndex(TargetIndex))
			{
				OutTarget = {Selection.Kind, Nodes[TargetIndex].ID};
				return true;
			}
		}
	}
	return false;
}

void SArcTacticsView::OpenAddPicker(const EArcTacticsEditorItemKind Kind)
{
	const FArcTacticsEditorSelection Parent = GetAddParent(Kind);
	if ((Kind != EArcTacticsEditorItemKind::Step
			&& Kind != EArcTacticsEditorItemKind::ActiveStep
			&& Kind != EArcTacticsEditorItemKind::Consideration)
		|| !Parent.IsConfig())
	{
		OpenTypePicker(Kind, Parent, false);
	}
}

bool SArcTacticsView::CanAddStep() const
{
	return !GetAddParent(EArcTacticsEditorItemKind::Step).IsConfig();
}

bool SArcTacticsView::CanAddActiveStep() const
{
	return !GetAddParent(EArcTacticsEditorItemKind::ActiveStep).IsConfig();
}

bool SArcTacticsView::CanAddConsideration() const
{
	return !GetAddParent(EArcTacticsEditorItemKind::Consideration).IsConfig();
}

void SArcTacticsView::OpenReplacePicker()
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (Model.IsValid() && CanReplaceSelection())
	{
		const FArcTacticsEditorSelection Item = Model->GetSelection();
		OpenTypePicker(Item.Kind, Item, true);
	}
}

bool SArcTacticsView::CanReplaceSelection() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (!Model.IsValid())
	{
		return false;
	}
	const FArcTacticsEditorSelection& Item = Model->GetSelection();
	return !Item.IsConfig()
		&& Item.Kind != EArcTacticsEditorItemKind::Context
		&& Model->IsSelectionValid(Item);
}

TSharedPtr<FArcTacticsViewItem> SArcTacticsView::FindItem(
	const FArcTacticsEditorSelection& Selection) const
{
	return FindItemRecursive(RootItems, Selection);
}

TSharedPtr<FArcTacticsViewItem> SArcTacticsView::FindItemRecursive(
	const TArray<TSharedPtr<FArcTacticsViewItem>>& Items,
	const FArcTacticsEditorSelection& Selection) const
{
	for (const TSharedPtr<FArcTacticsViewItem>& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		if (!Item->IsGroup() && Item->Selection == Selection)
		{
			return Item;
		}
		if (const TSharedPtr<FArcTacticsViewItem> Found = FindItemRecursive(Item->Children, Selection))
		{
			return Found;
		}
	}
	return nullptr;
}

bool SArcTacticsView::FindItemPathRecursive(
	const TArray<TSharedPtr<FArcTacticsViewItem>>& Items,
	const FArcTacticsEditorSelection& Selection,
	TArray<TSharedPtr<FArcTacticsViewItem>>& OutPath) const
{
	for (const TSharedPtr<FArcTacticsViewItem>& Item : Items)
	{
		if (!Item.IsValid())
		{
			continue;
		}
		OutPath.Add(Item);
		if ((!Item->IsGroup() && Item->Selection == Selection)
			|| FindItemPathRecursive(Item->Children, Selection, OutPath))
		{
			return true;
		}
		OutPath.Pop(EAllowShrinking::No);
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
