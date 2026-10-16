/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcAssetEditor_ItemDefinition.h"

#include "ArcItemDefinitionDetailCustomization.h"
#include "Customizations/ArcItemDefinitionFragmentFocusCustomization.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcAssetEditor_ItemDefinition"

const FName FArcAssetEditor_ItemDefinition::FragmentListTabId(TEXT("ItemEditor_FragmentList"));
const FName FArcAssetEditor_ItemDefinition::DetailsTabId(TEXT("ItemEditor_Details"));
const FName FArcAssetEditor_ItemDefinition::ToolkitFName(TEXT("ArcItemDefinitionEditor"));
const FName FArcAssetEditor_ItemDefinition::AppIdentifier(TEXT("ArcItemDefinitionEditorApp"));

namespace ArcAssetEditorItemDefinitionPrivate
{
	static void RehashSet(FArcItemDefinitionData& Data, FName SetName)
	{
		FSetProperty* SetProp = CastField<FSetProperty>(FArcItemDefinitionData::StaticStruct()->FindPropertyByName(SetName));
		if (SetProp == nullptr)
		{
			return;
		}
		FScriptSetHelper Helper(SetProp, SetProp->ContainerPtrToValuePtr<void>(&Data));
		Helper.Rehash();
	}

	static void PointAtRecord(const TSharedPtr<IStructureDetailsView>& View, UArcJsonRecordEditObject* Record)
	{
		if (!View.IsValid())
		{
			return;
		}
		if (Record == nullptr || !Record->GetRecord().IsValid())
		{
			View->SetStructureData(nullptr);
			return;
		}
		FInstancedStruct& Data = Record->GetMutableRecord();
		View->SetStructureData(MakeShared<FStructOnScope>(Data.GetScriptStruct(), Data.GetMutableMemory()));
	}
}

// ---------- Open / lifetime ----------

bool FArcAssetEditor_ItemDefinition::OpenRecord(const FString& Filename)
{
	if (UArcJsonRecordEditObject* OpenRecord = ArcJsonRecordEditing::FindRecord(Filename))
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor != nullptr ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
		if (AssetEditorSubsystem != nullptr && AssetEditorSubsystem->FindEditorForAsset(OpenRecord, true) != nullptr)
		{
			return true;
		}
	}

	UArcJsonRecordEditObject* EditRecord = ArcJsonRecordEditing::AcquireRecord(Filename);
	if (EditRecord == nullptr)
	{
		return false;
	}
	if (EditRecord->GetMutableRecord().GetMutablePtr<FArcItemDefinitionData>() == nullptr)
	{
		ArcJsonRecordEditing::ReleaseRecord(EditRecord);
		return false;
	}
	TSharedRef<FArcAssetEditor_ItemDefinition> NewEditor(new FArcAssetEditor_ItemDefinition());
	NewEditor->InitEditor(EditRecord);
	return true;
}

FArcAssetEditor_ItemDefinition::~FArcAssetEditor_ItemDefinition()
{
	ReleaseRecord();
}

void FArcAssetEditor_ItemDefinition::ReleaseRecord()
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		EditRecord->OnChanged().Remove(RecordChangedHandle);
		ArcJsonRecordEditing::ReleaseRecord(EditRecord);
	}
	RecordChangedHandle.Reset();
	Record.Reset();
}

// ---------- Init ----------

void FArcAssetEditor_ItemDefinition::InitEditor(UArcJsonRecordEditObject* InRecord)
{
	Record = InRecord;
	RecordChangedHandle = InRecord->OnChanged().AddSP(this, &FArcAssetEditor_ItemDefinition::HandleRecordChanged);

	FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs GeneralArgs;
	GeneralArgs.bAllowSearch = true;
	GeneralArgs.bShowOptions = false;
	GeneralArgs.bShowObjectLabel = false;
	GeneralArgs.bHideSelectionTip = true;
	GeneralArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	GeneralArgs.NotifyHook = this;
	GeneralDetailsView = PropModule.CreateStructureDetailView(GeneralArgs, FStructureDetailsViewArgs(), nullptr);
	GeneralDetailsView->GetDetailsView()->SetGenericLayoutDetailsDelegate(
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcItemDefinitionDetailCustomization::MakeInstance));

	FDetailsViewArgs FragArgs;
	FragArgs.bAllowSearch = false;
	FragArgs.bShowOptions = false;
	FragArgs.bShowObjectLabel = false;
	FragArgs.bHideSelectionTip = true;
	FragArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	FragArgs.NotifyHook = this;
	FragmentDetailsView = PropModule.CreateStructureDetailView(FragArgs, FStructureDetailsViewArgs(), nullptr);

	FragmentSelection = MakeShared<FArcItemFragmentSelection>();
	FragmentDetailsView->GetDetailsView()->SetGenericLayoutDetailsDelegate(
		FOnGetDetailCustomizationInstance::CreateLambda([SelectionWeak = TWeakPtr<FArcItemFragmentSelection>(FragmentSelection)]()
		{
			return FArcItemDefinitionFragmentFocusCustomization::MakeInstance(SelectionWeak);
		}));

	RefreshDetails();

	FragmentListWidget = SNew(SArcItemFragmentList, InRecord)
		.OnFragmentSelected(FArcOnFragmentSelected::CreateSP(
			this, &FArcAssetEditor_ItemDefinition::OnFragmentSelected));

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ArcItemDefinitionEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
					->AddTab(FragmentListTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->SetHideTabWell(true)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
			)
		);

	FAssetEditorToolkit::InitAssetEditor(
		EToolkitMode::Standalone,
		TSharedPtr<IToolkitHost>(),
		AppIdentifier,
		Layout,
		true,
		true,
		InRecord);

	RegenerateMenusAndToolbars();
}

// ---------- Tab Spawners ----------

void FArcAssetEditor_ItemDefinition::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_ItemEditor", "Item Definition Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(FragmentListTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_ItemDefinition::SpawnTab_FragmentList))
		.SetDisplayName(LOCTEXT("FragmentListTab", "Fragments"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_ItemDefinition::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FArcAssetEditor_ItemDefinition::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(FragmentListTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FArcAssetEditor_ItemDefinition::SpawnTab_FragmentList(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("FragmentListTitle", "Fragments"))
		[
			FragmentListWidget.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcAssetEditor_ItemDefinition::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTitle", "Details"))
		[
			SNew(SSplitter)
			.Orientation(Orient_Vertical)
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SAssignNew(DetailsSwitcher, SWidgetSwitcher)
				.WidgetIndex(FragmentSelection.IsValid() && FragmentSelection->SelectedStruct.IsValid() ? 1 : 0)
				+ SWidgetSwitcher::Slot()
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("NoSelection", "Select a fragment to edit its properties."))
					]
				]
				+ SWidgetSwitcher::Slot()
				[
					FragmentDetailsView->GetWidget().ToSharedRef()
				]
			]
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				GeneralDetailsView->GetWidget().ToSharedRef()
			]
		];
}

// ---------- Fragment Selection ----------

void FArcAssetEditor_ItemDefinition::OnFragmentSelected(
	const UScriptStruct* SelectedStruct,
	EArcFragmentSetType SetType)
{
	if (FragmentSelection.IsValid())
	{
		FragmentSelection->SelectedStruct = SelectedStruct;
		FragmentSelection->SetType = SetType;
	}

	ArcAssetEditorItemDefinitionPrivate::PointAtRecord(FragmentDetailsView, Record.Get());

	if (DetailsSwitcher.IsValid())
	{
		DetailsSwitcher->SetActiveWidgetIndex(SelectedStruct ? 1 : 0);
	}
}

// ---------- Record ----------

FArcItemDefinitionData* FArcAssetEditor_ItemDefinition::GetData() const
{
	UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr ? EditRecord->GetMutableRecord().GetMutablePtr<FArcItemDefinitionData>() : nullptr;
}

void FArcAssetEditor_ItemDefinition::HandleRecordChanged(UArcJsonRecordEditObject* Object, EArcJsonRecordEditChange Change)
{
	switch (Change)
	{
	case EArcJsonRecordEditChange::Edited:
	case EArcJsonRecordEditChange::Saved:
		break;
	case EArcJsonRecordEditChange::UndoRedo:
	case EArcJsonRecordEditChange::Reloaded:
		RefreshFromRecord();
		break;
	case EArcJsonRecordEditChange::Discarded:
	case EArcJsonRecordEditChange::Removed:
		if (!bClosing)
		{
			CloseWindow(EAssetEditorCloseReason::AssetUnloadingOrInvalid);
		}
		break;
	}
}

void FArcAssetEditor_ItemDefinition::RefreshFromRecord()
{
	RefreshDetails();
	if (FragmentListWidget.IsValid())
	{
		FragmentListWidget->RebuildTreeKeepingSelection();
	}
}

void FArcAssetEditor_ItemDefinition::RefreshDetails()
{
	ArcAssetEditorItemDefinitionPrivate::PointAtRecord(GeneralDetailsView, Record.Get());
	ArcAssetEditorItemDefinitionPrivate::PointAtRecord(FragmentDetailsView, Record.Get());
}

void FArcAssetEditor_ItemDefinition::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		EditRecord->Modify();
	}
}

void FArcAssetEditor_ItemDefinition::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		EditRecord->PostEditChange();
	}
}

void FArcAssetEditor_ItemDefinition::RunPreSave()
{
	FArcItemDefinitionData* Data = GetData();
	if (Data == nullptr)
	{
		return;
	}

	for (FArcInstancedStruct& IS : Data->ScalableFloatFragmentSet)
	{
		if (FArcScalableFloatItemFragment* Fragment = IS.GetMutablePtr<FArcScalableFloatItemFragment>())
		{
			Fragment->Initialize(IS.GetScriptStruct());
		}
		IS.PreSave();
	}

	for (FArcInstancedStruct& IS : Data->FragmentSet)
	{
		IS.PreSave();
	}

	ArcAssetEditorItemDefinitionPrivate::RehashSet(*Data, GET_MEMBER_NAME_CHECKED(FArcItemDefinitionData, FragmentSet));
	ArcAssetEditorItemDefinitionPrivate::RehashSet(*Data, GET_MEMBER_NAME_CHECKED(FArcItemDefinitionData, ScalableFloatFragmentSet));
}

bool FArcAssetEditor_ItemDefinition::SaveRecord()
{
	UArcJsonRecordEditObject* EditRecord = Record.Get();
	if (EditRecord == nullptr)
	{
		return false;
	}

	{
		const FScopedTransaction Transaction(LOCTEXT("SaveItemTransaction", "Save Item"));
		EditRecord->Modify();
		RunPreSave();
		EditRecord->PostEditChange();
	}

	if (!EditRecord->Save())
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("SaveFailed", "{0} could not be written. See the output log."), FText::FromString(EditRecord->GetFilename())),
			LOCTEXT("SaveFailedTitle", "Save failed"));
		return false;
	}
	return true;
}

// ---------- Save / close ----------

bool FArcAssetEditor_ItemDefinition::CanSaveAsset() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr && EditRecord->IsOpen();
}

void FArcAssetEditor_ItemDefinition::SaveAsset_Execute()
{
	SaveRecord();
}

void FArcAssetEditor_ItemDefinition::FindInContentBrowser_Execute()
{
	if (const UArcJsonRecordEditObject* Object = Record.Get())
	{
		ArcJsonRecordEditing::BrowseToRecordFile(Object->GetFilename());
	}
}

bool FArcAssetEditor_ItemDefinition::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	UArcJsonRecordEditObject* EditRecord = Record.Get();
	if (EditRecord == nullptr || !EditRecord->IsOpen() || !EditRecord->IsDirty() || DiscardOnCloseFrame == GFrameCounter)
	{
		return true;
	}
	const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNoCancel,
		FText::Format(LOCTEXT("SaveBeforeClose", "Save changes to {0}?"), FText::FromString(FPaths::GetCleanFilename(EditRecord->GetFilename()))),
		LOCTEXT("SaveBeforeCloseTitle", "Unsaved JSON asset"));
	if (Answer == EAppReturnType::Yes)
	{
		return SaveRecord();
	}
	if (Answer == EAppReturnType::No)
	{
		// Discard waits for the committed close: an editor exit can still be cancelled by a later prompt this frame.
		DiscardOnCloseFrame = GFrameCounter;
		return true;
	}
	return false;
}

void FArcAssetEditor_ItemDefinition::OnClose()
{
	bClosing = true;
	const bool bDiscard = DiscardOnCloseFrame == GFrameCounter;
	DiscardOnCloseFrame = MAX_uint64;
	UArcJsonRecordEditObject* EditRecord = Record.Get();
	if (bDiscard && EditRecord != nullptr && EditRecord->IsOpen())
	{
		EditRecord->Discard();
	}
	ReleaseRecord();
}

// ---------- Toolkit metadata ----------

FText FArcAssetEditor_ItemDefinition::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Item Definition Editor");
}

FText FArcAssetEditor_ItemDefinition::GetToolkitName() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr ? FText::FromString(FPaths::GetBaseFilename(EditRecord->GetFilename())) : GetBaseToolkitName();
}

FText FArcAssetEditor_ItemDefinition::GetTabSuffix() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr && EditRecord->IsDirty() ? LOCTEXT("DirtySuffix", "*") : FText::GetEmpty();
}

FText FArcAssetEditor_ItemDefinition::GetToolkitToolTipText() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr ? FText::FromString(EditRecord->GetFilename()) : GetBaseToolkitName();
}

FLinearColor FArcAssetEditor_ItemDefinition::GetWorldCentricTabColorScale() const
{
	return FLinearColor(1.0f, 0.6f, 0.0f, 0.5f);
}

FString FArcAssetEditor_ItemDefinition::GetWorldCentricTabPrefix() const
{
	return TEXT("ItemDef ");
}

#undef LOCTEXT_NAMESPACE
