// Copyright Lukasz Baran. All Rights Reserved.

#include "Widgets/SArcTacticsNodeRow.h"

#include "ArcTacticsEditorStyle.h"
#include "ArcTacticsModule.h"
#include "ArcTacticsNodePresentation.h"
#include "ArcTacticsNodeTypePicker.h"
#include "Decision/ArcTacticsConfig.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Styling/AppStyle.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SArcTacticsView.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SExpanderArrow.h"

#define LOCTEXT_NAMESPACE "SArcTacticsNodeRow"

namespace ArcTacticsNodeRowPrivate
{
	FArcTacticsNodePresentation GetPresentation(
		const TWeakPtr<FArcTacticsEditorViewModel>& WeakViewModel,
		const TSharedPtr<FArcTacticsViewItem>& Item)
	{
		const TSharedPtr<FArcTacticsEditorViewModel> Model = WeakViewModel.Pin();
		UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
		if (!Config || !Item.IsValid() || Item->IsGroup())
		{
			return FArcTacticsNodePresentation();
		}
		if (Item->Selection.Kind == EArcTacticsEditorItemKind::Context)
		{
			return FArcTacticsNodePresentation::BuildContext();
		}
		const FArcTacticsNodeDefinition* Node = Config->FindNodeByID(Item->Selection.ID);
		return Node
			? FArcTacticsNodePresentation::Build(*Node, *Config)
			: FArcTacticsNodePresentation();
	}

	FName GetAuthoredName(
		const TWeakPtr<FArcTacticsEditorViewModel>& WeakViewModel,
		const TSharedPtr<FArcTacticsViewItem>& Item)
	{
		const TSharedPtr<FArcTacticsEditorViewModel> Model = WeakViewModel.Pin();
		UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
		if (!Config || !Item.IsValid() || Item->IsGroup())
		{
			return NAME_None;
		}
		const FArcTacticsNodeDefinition* Node = Config->FindNodeByID(Item->Selection.ID);
		return Node ? Node->Name : NAME_None;
	}

	TOptional<EArcTacticsEditorItemKind> GetGroupAddKind(const TSharedPtr<FArcTacticsViewItem>& Item)
	{
		if (!Item.IsValid())
		{
			return TOptional<EArcTacticsEditorItemKind>();
		}
		switch (Item->Group)
		{
		case EArcTacticsViewGroup::GlobalTasks:
			return EArcTacticsEditorItemKind::GlobalTask;
		case EArcTacticsViewGroup::Actions:
			return EArcTacticsEditorItemKind::Action;
		case EArcTacticsViewGroup::Selectors:
			return EArcTacticsEditorItemKind::Selector;
		case EArcTacticsViewGroup::ActiveSteps:
			return EArcTacticsEditorItemKind::ActiveStep;
		default:
			return TOptional<EArcTacticsEditorItemKind>();
		}
	}

	class FDragDropOperation final : public FDecoratedDragDropOp
	{
	public:
		DRAG_DROP_OPERATOR_TYPE(FDragDropOperation, FDecoratedDragDropOp);

		static TSharedRef<FDragDropOperation> New(
			const FArcTacticsEditorSelection& InSelection,
			const FText& InLabel,
			TWeakPtr<FArcTacticsEditorViewModel> InViewModel)
		{
			TSharedRef<FDragDropOperation> Operation = MakeShared<FDragDropOperation>();
			Operation->Selection = InSelection;
			Operation->Label = InLabel;
			Operation->ViewModel = InViewModel;
			Operation->SetCanDrop(false);
			Operation->FDecoratedDragDropOp::Construct();
			return Operation;
		}

		void SetCanDrop(const bool bCanDrop)
		{
			const FSlateBrush* Icon = FAppStyle::GetBrush(
				bCanDrop ? TEXT("Icons.Check") : TEXT("Icons.Error"));
			SetToolTip(Label, Icon);
		}

		FArcTacticsEditorSelection Selection;
		FText Label;
		TWeakPtr<FArcTacticsEditorViewModel> ViewModel;
	};
}

void SArcTacticsNodeRow::Construct(
	const FArguments& InArgs,
	const TSharedRef<STableViewBase>& OwnerTable,
	TSharedPtr<FArcTacticsViewItem> InItem,
	TWeakPtr<FArcTacticsEditorViewModel> InViewModel)
{
	Item = MoveTemp(InItem);
	ViewModel = MoveTemp(InViewModel);
	OnGetTypePicker = InArgs._OnGetTypePicker;

	ConstructInternal(
		STableRow::FArguments()
		.OnDragDetected(this, &SArcTacticsNodeRow::HandleDragDetected)
		.OnDragLeave(this, &SArcTacticsNodeRow::HandleDragLeave)
		.OnCanAcceptDrop(this, &SArcTacticsNodeRow::HandleCanAcceptDrop)
		.OnAcceptDrop(this, &SArcTacticsNodeRow::HandleAcceptDrop),
		OwnerTable);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(this, &SArcTacticsNodeRow::GetBackgroundBrush)
		.Padding(FMargin(6.0f, 4.0f))
		[
			SNew(SHorizontalBox)
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
			.Padding(2.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SImage)
				.Image(this, &SArcTacticsNodeRow::GetIcon)
				.ColorAndOpacity(this, &SArcTacticsNodeRow::GetIconColor)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SRichTextBlock)
				.Visibility(this, &SArcTacticsNodeRow::GetLabelVisibility)
				.Text(this, &SArcTacticsNodeRow::GetLabel)
				.ToolTipText(this, &SArcTacticsNodeRow::GetTooltip)
				.TextStyle(&FArcTacticsEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(
					TEXT("ArcTactics.Node.Normal")))
				+ SRichTextBlock::Decorator(FArcTacticsNodePresentation::CreateTextDecorator(NAME_None))
				+ SRichTextBlock::Decorator(FArcTacticsNodePresentation::CreateTextDecorator(TEXT("b")))
				+ SRichTextBlock::Decorator(FArcTacticsNodePresentation::CreateTextDecorator(TEXT("s")))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(RenameText, SInlineEditableTextBlock)
				.Visibility(this, &SArcTacticsNodeRow::GetRenameVisibility)
				.Style(&FArcTacticsEditorStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>(
					TEXT("ArcTactics.Node.TitleInlineEditableText")))
				.Text(this, &SArcTacticsNodeRow::GetRenameText)
				.OnVerifyTextChanged(this, &SArcTacticsNodeRow::HandleVerifyName)
				.OnTextCommitted(this, &SArcTacticsNodeRow::HandleNameCommitted)
				.IsSelected(this, &SArcTacticsNodeRow::IsNodeSelected)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.Visibility(this, &SArcTacticsNodeRow::GetTypePickerVisibility)
				.OnGetMenuContent(OnGetTypePicker)
				.OnMenuOpenChanged_Lambda([this](const bool bIsOpen)
				{
					UE_LOGF(
						LogArcTactics,
						Log,
						"[ArcTacticsPicker] NodeType Surface=Row State=%ls Node=%ls",
						bIsOpen ? TEXT("Opened") : TEXT("Closed"),
						Item.IsValid() ? *Item->Selection.ID.ToString() : TEXT("<invalid>"));
				})
				.HasDownArrow(false)
				.ContentPadding(FMargin(3.0f))
				.ToolTipText(LOCTEXT("ChangeTypeTooltip", "Change node type"))
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Icons.Edit")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.Visibility(this, &SArcTacticsNodeRow::GetAddGroupItemVisibility)
				.IsFocusable(false)
				.OnGetMenuContent(this, &SArcTacticsNodeRow::CreateAddGroupItemPicker)
				.HasDownArrow(false)
				.ContentPadding(FMargin(3.0f))
				.ToolTipText(this, &SArcTacticsNodeRow::GetAddGroupItemTooltip)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Icons.Plus")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.Visibility(this, &SArcTacticsNodeRow::GetAddBranchNodeVisibility)
				.IsEnabled(this, &SArcTacticsNodeRow::CanAddBranchNode)
				.IsFocusable(false)
				.OnGetMenuContent(this, &SArcTacticsNodeRow::CreateAddStepPicker)
				.HasDownArrow(false)
				.ContentPadding(FMargin(3.0f))
				.ToolTipText(LOCTEXT("AddStepTooltip", "Add Step"))
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Icons.Plus")))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SComboButton)
				.Visibility(this, &SArcTacticsNodeRow::GetAddBranchNodeVisibility)
				.IsEnabled(this, &SArcTacticsNodeRow::CanAddBranchNode)
				.IsFocusable(false)
				.OnGetMenuContent(this, &SArcTacticsNodeRow::CreateAddConsiderationPicker)
				.HasDownArrow(false)
				.ContentPadding(FMargin(3.0f))
				.ToolTipText(LOCTEXT("AddConsiderationTooltip", "Add Consideration"))
				.ButtonContent()
				[
					SNew(SImage)
					.Image_Static(&ArcTacticsNodeTypePicker::GetAddConsiderationIcon)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Visibility(this, &SArcTacticsNodeRow::GetDeleteBranchNodeVisibility)
				.IsEnabled(this, &SArcTacticsNodeRow::CanDeleteBranchNode)
				.IsFocusable(false)
				.OnClicked(this, &SArcTacticsNodeRow::HandleDeleteBranchNodeClicked)
				.ContentPadding(FMargin(3.0f))
				.ToolTipText(this, &SArcTacticsNodeRow::GetDeleteBranchNodeTooltip)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(TEXT("Icons.Delete")))
				]
			]
		]
	];
}

void SArcTacticsNodeRow::RequestRename()
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (!Item.IsValid() || Item->IsGroup() || !Model.IsValid()
		|| !Model->CanRename(Item->Selection))
	{
		return;
	}
	bRenaming = true;
	Invalidate(EInvalidateWidgetReason::Layout);
	if (RenameText.IsValid())
	{
		RenameText->EnterEditingMode();
	}
}

FText SArcTacticsNodeRow::GetLabel() const
{
	if (!Item.IsValid())
	{
		return FText::GetEmpty();
	}
	return Item->IsGroup()
		? Item->Label
		: ArcTacticsNodeRowPrivate::GetPresentation(ViewModel, Item).RichText;
}

FText SArcTacticsNodeRow::GetPlainLabel() const
{
	if (!Item.IsValid())
	{
		return FText::GetEmpty();
	}
	return Item->IsGroup()
		? Item->Label
		: ArcTacticsNodeRowPrivate::GetPresentation(ViewModel, Item).PlainText;
}

FText SArcTacticsNodeRow::GetRenameText() const
{
	return FText::FromName(ArcTacticsNodeRowPrivate::GetAuthoredName(ViewModel, Item));
}

FText SArcTacticsNodeRow::GetTooltip() const
{
	return GetPlainLabel();
}

const FSlateBrush* SArcTacticsNodeRow::GetIcon() const
{
	if (Item.IsValid() && Item->IsGroup())
	{
		return FAppStyle::GetBrush(TEXT("Icons.FolderClosed"));
	}
	return ArcTacticsNodeRowPrivate::GetPresentation(ViewModel, Item).Icon.GetIcon();
}

FSlateColor SArcTacticsNodeRow::GetIconColor() const
{
	if (Item.IsValid() && Item->IsGroup())
	{
		return FSlateColor::UseSubduedForeground();
	}
	return ArcTacticsNodeRowPrivate::GetPresentation(ViewModel, Item).IconColor;
}

const FSlateBrush* SArcTacticsNodeRow::GetBackgroundBrush() const
{
	FName StyleName(TEXT("ArcTactics.Node.Row"));
	if (bDropTarget)
	{
		StyleName = bDropAllowed
			? TEXT("ArcTactics.Node.Row.DropTarget")
			: TEXT("ArcTactics.Node.Row.Error");
	}
	else if (HasInvalidData())
	{
		StyleName = TEXT("ArcTactics.Node.Row.Error");
	}
	else if (IsSelected())
	{
		StyleName = HasKeyboardFocus() || HasFocusedDescendants()
			? TEXT("ArcTactics.Node.Row.Focused")
			: TEXT("ArcTactics.Node.Row.Selected");
	}
	else if (IsHovered())
	{
		StyleName = TEXT("ArcTactics.Node.Row.Hovered");
	}
	return FArcTacticsEditorStyle::Get().GetBrush(StyleName);
}

EVisibility SArcTacticsNodeRow::GetLabelVisibility() const
{
	return bRenaming ? EVisibility::Collapsed : EVisibility::Visible;
}

EVisibility SArcTacticsNodeRow::GetRenameVisibility() const
{
	return bRenaming ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SArcTacticsNodeRow::GetTypePickerVisibility() const
{
	return Item.IsValid() && !Item->IsGroup()
		&& Item->Selection.Kind != EArcTacticsEditorItemKind::Context
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SArcTacticsNodeRow::GetAddBranchNodeVisibility() const
{
	return Item.IsValid() && !Item->IsGroup()
		&& (Item->Selection.Kind == EArcTacticsEditorItemKind::Action
			|| Item->Selection.Kind == EArcTacticsEditorItemKind::SelectorAction)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

EVisibility SArcTacticsNodeRow::GetDeleteBranchNodeVisibility() const
{
	return Item.IsValid() && !Item->IsGroup()
		&& (Item->Selection.Kind == EArcTacticsEditorItemKind::Step
			|| Item->Selection.Kind == EArcTacticsEditorItemKind::ActiveStep
			|| Item->Selection.Kind == EArcTacticsEditorItemKind::Consideration)
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

bool SArcTacticsNodeRow::CanAddBranchNode() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Item.IsValid() && !Item->IsGroup()
		&& (Item->Selection.Kind == EArcTacticsEditorItemKind::Action
			|| Item->Selection.Kind == EArcTacticsEditorItemKind::SelectorAction)
		&& Model->IsSelectionValid(Item->Selection);
}

bool SArcTacticsNodeRow::CanDeleteBranchNode() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	return Model.IsValid() && Item.IsValid() && !Item->IsGroup()
		&& (Item->Selection.Kind == EArcTacticsEditorItemKind::Step
			|| Item->Selection.Kind == EArcTacticsEditorItemKind::ActiveStep
			|| Item->Selection.Kind == EArcTacticsEditorItemKind::Consideration)
		&& Model->CanDelete(Item->Selection);
}

FText SArcTacticsNodeRow::GetDeleteBranchNodeTooltip() const
{
	if (!Item.IsValid())
	{
		return FText::GetEmpty();
	}
	switch (Item->Selection.Kind)
	{
	case EArcTacticsEditorItemKind::Step:
		return LOCTEXT("DeleteStepTooltip", "Delete Step");
	case EArcTacticsEditorItemKind::ActiveStep:
		return LOCTEXT("DeleteActiveStepTooltip", "Delete While Active Step");
	default:
		return LOCTEXT("DeleteConsiderationTooltip", "Delete Consideration");
	}
}

EVisibility SArcTacticsNodeRow::GetAddGroupItemVisibility() const
{
	return ArcTacticsNodeRowPrivate::GetGroupAddKind(Item).IsSet()
		? EVisibility::Visible
		: EVisibility::Collapsed;
}

FText SArcTacticsNodeRow::GetAddGroupItemTooltip() const
{
	const TOptional<EArcTacticsEditorItemKind> Kind = ArcTacticsNodeRowPrivate::GetGroupAddKind(Item);
	if (!Kind.IsSet())
	{
		return FText::GetEmpty();
	}
	switch (Kind.GetValue())
	{
	case EArcTacticsEditorItemKind::GlobalTask:
		return LOCTEXT("AddGlobalTaskTooltip", "Add Global Task");
	case EArcTacticsEditorItemKind::Action:
		return LOCTEXT("AddActionTooltip", "Add Action");
	case EArcTacticsEditorItemKind::ActiveStep:
		return LOCTEXT("AddActiveStepTooltip", "Add While Active Step");
	default:
		return LOCTEXT("AddSelectorTooltip", "Add Selector");
	}
}

TSharedRef<SWidget> SArcTacticsNodeRow::CreateAddGroupItemPicker()
{
	const TOptional<EArcTacticsEditorItemKind> Kind = ArcTacticsNodeRowPrivate::GetGroupAddKind(Item);
	if (!Kind.IsSet())
	{
		return SNullWidget::NullWidget;
	}
	return ArcTacticsNodeTypePicker::MakeNodeTypePicker(
		ViewModel,
		Kind.GetValue(),
		Item->Group == EArcTacticsViewGroup::ActiveSteps
			? Item->Selection
			: FArcTacticsEditorSelection::Config(),
		false,
		EArcTacticsNodeTypePickerSurface::View);
}

TSharedRef<SWidget> SArcTacticsNodeRow::CreateAddStepPicker()
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}
	return ArcTacticsNodeTypePicker::MakeNodeTypePicker(
		ViewModel,
		EArcTacticsEditorItemKind::Step,
		Item->Selection,
		false,
		EArcTacticsNodeTypePickerSurface::View);
}

TSharedRef<SWidget> SArcTacticsNodeRow::CreateAddConsiderationPicker()
{
	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}
	return ArcTacticsNodeTypePicker::MakeNodeTypePicker(
		ViewModel,
		EArcTacticsEditorItemKind::Consideration,
		Item->Selection,
		false,
		EArcTacticsNodeTypePickerSurface::View);
}

FReply SArcTacticsNodeRow::HandleDeleteBranchNodeClicked()
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (Model.IsValid() && Item.IsValid())
	{
		Model->Delete(Item->Selection);
	}
	return FReply::Handled();
}

bool SArcTacticsNodeRow::IsNodeSelected() const
{
	return IsSelected();
}

bool SArcTacticsNodeRow::HasInvalidData() const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	UArcTacticsConfig* Config = Model.IsValid() ? Model->GetConfig() : nullptr;
	if (!Config || !Item.IsValid() || Item->IsGroup())
	{
		return false;
	}
	if (Item->Selection.Kind == EArcTacticsEditorItemKind::Context)
	{
		return !Model->IsSelectionValid(Item->Selection);
	}
	const FArcTacticsNodeDefinition* Node = Config->FindNodeByID(Item->Selection.ID);
	return !Node || !Node->Node.GetPtr<FArcTacticsNode>();
}

bool SArcTacticsNodeRow::HandleVerifyName(const FText& Text, FText& OutError) const
{
	const FString Name = FText::TrimPrecedingAndTrailing(Text).ToString();
	if (Name.Len() >= NAME_SIZE)
	{
		OutError = LOCTEXT("NameTooLong", "Name is too long.");
		return false;
	}
	return true;
}

void SArcTacticsNodeRow::HandleNameCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	bRenaming = false;
	Invalidate(EInvalidateWidgetReason::Layout);
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (Model.IsValid() && Item.IsValid())
	{
		const FString Name = FText::TrimPrecedingAndTrailing(Text).ToString();
		Model->Rename(Item->Selection, FName(*Name));
	}
}

FReply SArcTacticsNodeRow::HandleDragDetected(
	const FGeometry& Geometry,
	const FPointerEvent& PointerEvent) const
{
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	if (!PointerEvent.IsMouseButtonDown(EKeys::LeftMouseButton)
		|| !Model.IsValid() || !Item.IsValid() || Item->IsGroup()
		|| Item->Selection.Kind == EArcTacticsEditorItemKind::Context
		|| Item->Selection.Kind == EArcTacticsEditorItemKind::SelectorAction)
	{
		return FReply::Unhandled();
	}
	Model->SetSelection(Item->Selection);
	return FReply::Handled().BeginDragDrop(ArcTacticsNodeRowPrivate::FDragDropOperation::New(
		Item->Selection, GetPlainLabel(), ViewModel));
}

void SArcTacticsNodeRow::HandleDragLeave(const FDragDropEvent& DragDropEvent)
{
	ClearDropState();
}

TOptional<EItemDropZone> SArcTacticsNodeRow::HandleCanAcceptDrop(
	const FDragDropEvent& DragDropEvent,
	const EItemDropZone DropZone,
	TSharedPtr<FArcTacticsViewItem> TargetItem)
{
	const TSharedPtr<ArcTacticsNodeRowPrivate::FDragDropOperation> Operation =
		DragDropEvent.GetOperationAs<ArcTacticsNodeRowPrivate::FDragDropOperation>();
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	const TSharedPtr<FArcTacticsEditorViewModel> OriginModel =
		Operation.IsValid() ? Operation->ViewModel.Pin() : nullptr;
	bDropTarget = Operation.IsValid() && Model.IsValid() && TargetItem.IsValid() && !TargetItem->IsGroup()
		&& TargetItem->Selection.Kind != EArcTacticsEditorItemKind::Context;
	bDropAllowed = false;
	if (bDropTarget)
	{
		if (OriginModel == Model)
		{
			const TOptional<EArcTacticsEditorInsertLocation> Location = ResolveDropLocation(
				DropZone, Operation->Selection, TargetItem->Selection);
			if (Location.IsSet())
			{
				DropLocation = Location.GetValue();
				bDropAllowed = Model->CanMove(
					Operation->Selection, TargetItem->Selection, DropLocation);
			}
		}
		Operation->SetCanDrop(bDropAllowed);
	}
	Invalidate(EInvalidateWidgetReason::Paint);
	return bDropAllowed ? TOptional<EItemDropZone>(DropZone) : TOptional<EItemDropZone>();
}

FReply SArcTacticsNodeRow::HandleAcceptDrop(
	const FDragDropEvent& DragDropEvent,
	const EItemDropZone DropZone,
	TSharedPtr<FArcTacticsViewItem> TargetItem)
{
	const TSharedPtr<ArcTacticsNodeRowPrivate::FDragDropOperation> Operation =
		DragDropEvent.GetOperationAs<ArcTacticsNodeRowPrivate::FDragDropOperation>();
	const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin();
	const TSharedPtr<FArcTacticsEditorViewModel> OriginModel =
		Operation.IsValid() ? Operation->ViewModel.Pin() : nullptr;
	const bool bMoved = bDropAllowed && Operation.IsValid() && Model.IsValid()
		&& OriginModel == Model && TargetItem.IsValid()
		&& Model->Move(Operation->Selection, TargetItem->Selection, DropLocation);
	ClearDropState();
	return bMoved ? FReply::Handled() : FReply::Unhandled();
}

TOptional<EArcTacticsEditorInsertLocation> SArcTacticsNodeRow::ResolveDropLocation(
	const EItemDropZone DropZone,
	const FArcTacticsEditorSelection& Source,
	const FArcTacticsEditorSelection& Target) const
{
	if (DropZone == EItemDropZone::AboveItem)
	{
		return EArcTacticsEditorInsertLocation::Before;
	}
	if (DropZone == EItemDropZone::BelowItem)
	{
		return EArcTacticsEditorInsertLocation::After;
	}
	if ((Source.Kind == EArcTacticsEditorItemKind::Step || Source.Kind == EArcTacticsEditorItemKind::ActiveStep)
		&& (Target.Kind == EArcTacticsEditorItemKind::Action
			|| Target.Kind == EArcTacticsEditorItemKind::SelectorAction))
	{
		return EArcTacticsEditorInsertLocation::Into;
	}
	if (Source.Kind == EArcTacticsEditorItemKind::Consideration
		&& (Target.Kind == EArcTacticsEditorItemKind::Action
			|| Target.Kind == EArcTacticsEditorItemKind::Selector
			|| Target.Kind == EArcTacticsEditorItemKind::SelectorAction))
	{
		return EArcTacticsEditorInsertLocation::Into;
	}
	return TOptional<EArcTacticsEditorInsertLocation>();
}

void SArcTacticsNodeRow::ClearDropState()
{
	bDropTarget = false;
	bDropAllowed = false;
	Invalidate(EInvalidateWidgetReason::Paint);
}

#undef LOCTEXT_NAMESPACE
