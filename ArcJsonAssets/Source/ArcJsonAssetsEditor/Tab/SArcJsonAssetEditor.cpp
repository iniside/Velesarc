// Copyright Lukasz Baran. All Rights Reserved.

#include "Tab/SArcJsonAssetEditor.h"

#include "CoreGlobals.h"
#include "DetailsViewArgs.h"
#include "IStructureDetailsView.h"
#include "InputCoreTypes.h"
#include "JsonAssets/ArcJsonAssetPaths.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcJsonAssetsEditor"

SArcJsonAssetEditor::~SArcJsonAssetEditor()
{
	UnbindRecord();
}

void SArcJsonAssetEditor::Construct(const FArguments& InArgs)
{
	Filename = InArgs._Filename;
	OnRecordClosed = InArgs._OnRecordClosed;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.NotifyHook = this;
	FStructureDetailsViewArgs StructureDetailsViewArgs;
	DetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureDetailsViewArgs, nullptr);

	BindRecord(ArcJsonRecordEditing::AcquireRecord(Filename));

	FString DisplayPath;
	if (!ArcJsonAssetPaths::ToPackagePath(ArcJsonAssetPaths::GetContentRoots(), Filename, DisplayPath))
	{
		DisplayPath = Filename;
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
			.Padding(FMargin(4.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Save", "Save"))
					.ToolTipText(LOCTEXT("SaveTooltip", "Write the record to its JSON file (Ctrl+S)"))
					.IsEnabled_Lambda([this]() -> bool
					{
						return Record.IsValid();
					})
					.OnClicked(this, &SArcJsonAssetEditor::HandleSaveClicked)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("FindInContentBrowser", "Find in Content Browser"))
					.IsEnabled_Lambda([this]() -> bool
					{
						return Record.IsValid();
					})
					.OnClicked_Lambda([this]() -> FReply
					{
						ArcJsonRecordEditing::BrowseToRecordFile(Filename);
						return FReply::Handled();
					})
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush(TEXT("SystemWideCommands.FindInContentBrowser.Small")))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(FMargin(8.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(DisplayPath))
					.ToolTipText(FText::FromString(Filename))
				]
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			DetailsView->GetWidget().ToSharedRef()
		]
	];
}

bool SArcJsonAssetEditor::IsLoaded() const
{
	return Record.IsValid();
}

FText SArcJsonAssetEditor::GetTabLabel() const
{
	const FString BaseName = FPaths::GetBaseFilename(Filename);
	const UArcJsonRecordEditObject* Object = Record.Get();
	return FText::FromString(Object && Object->IsDirty() ? BaseName + TEXT("*") : BaseName);
}

bool SArcJsonAssetEditor::CanClose()
{
	UArcJsonRecordEditObject* Object = Record.Get();
	if (!Object || !Object->IsDirty() || DiscardOnCloseFrame == GFrameCounter)
	{
		return true;
	}
	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNoCancel,
		FText::Format(LOCTEXT("SaveBeforeClose", "Save changes to {0}?"), FText::FromString(FPaths::GetCleanFilename(Filename))),
		LOCTEXT("SaveBeforeCloseTitle", "Unsaved JSON asset"));
	if (Answer == EAppReturnType::Yes)
	{
		return Save();
	}
	if (Answer == EAppReturnType::No)
	{
		// Discard waits for the tab to close: an editor exit can still be cancelled by a later prompt this frame.
		DiscardOnCloseFrame = GFrameCounter;
		return true;
	}
	return false;
}

void SArcJsonAssetEditor::HandleTabClosed()
{
	const bool bDiscard = DiscardOnCloseFrame == GFrameCounter;
	DiscardOnCloseFrame = MAX_uint64;
	bClosing = true;
	UArcJsonRecordEditObject* Object = Record.Get();
	if (bDiscard && Object && Object->IsOpen())
	{
		Object->Discard();
	}
	UnbindRecord();
}

bool SArcJsonAssetEditor::Save()
{
	UArcJsonRecordEditObject* Object = Record.Get();
	if (!Object)
	{
		return false;
	}
	if (!Object->Save())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("SaveFailed", "{0} could not be written. See the output log."), FText::FromString(Filename)),
			LOCTEXT("SaveFailedTitle", "Save failed"));
		return false;
	}
	return true;
}

bool SArcJsonAssetEditor::SupportsKeyboardFocus() const
{
	return true;
}

FReply SArcJsonAssetEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::S && InKeyEvent.IsControlDown() && !InKeyEvent.IsAltDown() && !InKeyEvent.IsShiftDown())
	{
		Save();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SArcJsonAssetEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	if (UArcJsonRecordEditObject* Object = Record.Get())
	{
		Object->Modify();
	}
}

void SArcJsonAssetEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (UArcJsonRecordEditObject* Object = Record.Get())
	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		Object->PostEditChange();
	}
}

void SArcJsonAssetEditor::BindRecord(UArcJsonRecordEditObject* Object)
{
	Record = Object;
	if (Object)
	{
		RecordChangedHandle = Object->OnChanged().AddSP(this, &SArcJsonAssetEditor::HandleRecordChanged);
	}
	RefreshDetails();
}

void SArcJsonAssetEditor::UnbindRecord()
{
	if (DetailsView.IsValid())
	{
		DetailsView->SetStructureData(nullptr);
	}
	if (UArcJsonRecordEditObject* Object = Record.Get())
	{
		Object->OnChanged().Remove(RecordChangedHandle);
		ArcJsonRecordEditing::ReleaseRecord(Object);
	}
	RecordChangedHandle.Reset();
	Record.Reset();
}

void SArcJsonAssetEditor::RefreshDetails()
{
	UArcJsonRecordEditObject* Object = Record.Get();
	if (!Object || !Object->GetRecord().IsValid())
	{
		DetailsView->SetStructureData(nullptr);
		return;
	}
	FInstancedStruct& Data = Object->GetMutableRecord();
	DetailsView->SetStructureData(MakeShared<FStructOnScope>(Data.GetScriptStruct(), Data.GetMutableMemory()));
}

void SArcJsonAssetEditor::HandleRecordChanged(UArcJsonRecordEditObject* Object, EArcJsonRecordEditChange Change)
{
	switch (Change)
	{
	case EArcJsonRecordEditChange::Edited:
		if (!bApplyingEdit)
		{
			RefreshDetails();
		}
		break;
	case EArcJsonRecordEditChange::UndoRedo:
	case EArcJsonRecordEditChange::Reloaded:
		RefreshDetails();
		break;
	case EArcJsonRecordEditChange::Saved:
		break;
	case EArcJsonRecordEditChange::Discarded:
		UnbindRecord();
		if (!bClosing)
		{
			BindRecord(ArcJsonRecordEditing::AcquireRecord(Filename));
			if (!Record.IsValid())
			{
				OnRecordClosed.ExecuteIfBound();
			}
		}
		break;
	case EArcJsonRecordEditChange::Removed:
		UnbindRecord();
		OnRecordClosed.ExecuteIfBound();
		break;
	}
}

FReply SArcJsonAssetEditor::HandleSaveClicked()
{
	Save();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
