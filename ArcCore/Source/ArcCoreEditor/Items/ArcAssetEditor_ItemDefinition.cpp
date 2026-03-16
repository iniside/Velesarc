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

#include "ArcCore/Items/ArcItemDefinition.h"
#include "ArcCoreEditor/ArcCoreEditorModule.h"
#include "Items/ArcItemJsonSerializer.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "DesktopPlatformModule.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Misc/FileHelper.h"
#include "Widgets/Layout/SSplitter.h"
#include "UObject/StructOnScope.h"
#include "ArcStaticItemHelpers.h"
#include "Editor.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Input/SHyperlink.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "ArcAssetEditor_ItemDefinition"

const FName FArcAssetEditor_ItemDefinition::FragmentListTabId(TEXT("ItemEditor_FragmentList"));
const FName FArcAssetEditor_ItemDefinition::DetailsTabId(TEXT("ItemEditor_Details"));
const FName FArcAssetEditor_ItemDefinition::ToolkitFName(TEXT("ArcItemDefinitionEditor"));
const FName FArcAssetEditor_ItemDefinition::AppIdentifier(TEXT("ArcItemDefinitionEditorApp"));

// ---------- Factory ----------

TSharedRef<FArcAssetEditor_ItemDefinition> FArcAssetEditor_ItemDefinition::CreateItemEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UObject* ObjectToEdit)
{
	TSharedRef<FArcAssetEditor_ItemDefinition> NewEditor(new FArcAssetEditor_ItemDefinition());
	UArcItemDefinition* ItemDef = CastChecked<UArcItemDefinition>(ObjectToEdit);
	NewEditor->InitItemEditor(Mode, InitToolkitHost, ItemDef);
	return NewEditor;
}

TSharedRef<FArcAssetEditor_ItemDefinition> FArcAssetEditor_ItemDefinition::CreateItemEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	const TArray<UObject*>& ObjectsToEdit)
{
	// Only supports single object editing
	check(ObjectsToEdit.Num() > 0);
	return CreateItemEditor(Mode, InitToolkitHost, ObjectsToEdit[0]);
}

// ---------- Init ----------

void FArcAssetEditor_ItemDefinition::InitItemEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcItemDefinition* ObjectToEdit)
{
	// Create general details view (shows non-fragment item properties)
	FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs GeneralArgs;
	GeneralArgs.bAllowSearch = true;
	GeneralArgs.bShowOptions = false;
	GeneralArgs.bShowObjectLabel = false;
	GeneralArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	GeneralDetailsView = PropModule.CreateDetailView(GeneralArgs);
	GeneralDetailsView->SetObject(ObjectToEdit);

	// Create fragment details view (shows selected fragment properties)
	FDetailsViewArgs FragArgs;
	FragArgs.bShowOptions = false;
	FragArgs.bAllowSearch = false;
	FragArgs.bShowObjectLabel = false;
	FragArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FStructureDetailsViewArgs StructArgs;
	FragmentDetailsView = PropModule.CreateStructureDetailView(FragArgs, StructArgs, nullptr);
	FragmentDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(
		this, &FArcAssetEditor_ItemDefinition::OnFragmentPropertyChanged);

	// Create fragment list widget (left panel)
	FragmentListWidget = SNew(SArcItemFragmentList, ObjectToEdit)
		.OnFragmentSelected(FArcOnFragmentSelected::CreateSP(
			this, &FArcAssetEditor_ItemDefinition::OnFragmentSelected));

	// Define two-panel layout
	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout("ArcItemDefinitionEditor_Layout_v1")
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

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(ObjectToEdit);

	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		AppIdentifier,
		Layout,
		true,  // bCreateDefaultStandaloneMenu
		true,  // bCreateDefaultToolbar
		ObjectsToEdit);

	ExtendToolbar();
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
				GeneralDetailsView.ToSharedRef()
			]
		];
}

void FArcAssetEditor_ItemDefinition::PostRegenerateMenusAndToolbars()
{
	// Intentionally skip FSimpleAssetEditor::PostRegenerateMenusAndToolbars()
	// which sets up a default property panel using its private EditingObjects.
	// We have our own custom fragment/details views instead.
}

// ---------- Toolbar ----------

void FArcAssetEditor_ItemDefinition::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
		{
			// Template operations are only relevant for non-template items
			UArcItemDefinition* ItemDef = GetEditingItemDefinition();
			if (ItemDef && ItemDef->IsA<UArcItemDefinitionTemplate>())
			{
				return;
			}

			ToolbarBuilder.BeginSection("Template");
			{
				ToolbarBuilder.AddWidget(
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(4.f, 0.f, 2.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TemplateToolbarLabel", "Template:"))
						.Font(FAppStyle::GetFontStyle("BoldFont"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SHyperlink)
						.Text(this, &FArcAssetEditor_ItemDefinition::GetTemplateNameText)
						.OnNavigate_Lambda([this]()
						{
							if (UArcItemDefinition* ItemDef = GetEditingItemDefinition())
							{
								if (UArcItemDefinitionTemplate* Template = ItemDef->GetSourceTemplate())
								{
									GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Template);
								}
							}
						})
					]
				);

				ToolbarBuilder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([this]() { OnChangeTemplateClicked(); })
					),
					NAME_None,
					LOCTEXT("ChangeTemplateBtn", "Change Template"),
					LOCTEXT("ChangeTemplateBtnTip", "Pick a new template for this item"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import")
				);

				ToolbarBuilder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([this]() { OnUpdateFromTemplateClicked(); }),
						FCanExecuteAction::CreateLambda([this]() { return HasSourceTemplate(); })
					),
					NAME_None,
					LOCTEXT("UpdateFromTemplateBtn", "Update"),
					LOCTEXT("UpdateFromTemplateBtnTip", "Update fragments from the source template"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh")
				);

				ToolbarBuilder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([this]() { OnPushToTemplateClicked(); }),
						FCanExecuteAction::CreateLambda([this]() { return HasSourceTemplate(); })
					),
					NAME_None,
					LOCTEXT("PushToTemplateBtn", "Push to Template"),
					LOCTEXT("PushToTemplateBtnTip", "Overwrite the source template with this item's fragments"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Export")
				);
			}
			ToolbarBuilder.EndSection();

			// JSON import/export section (available for all item types)
			ToolbarBuilder.BeginSection("JSON");
			{
				ToolbarBuilder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([this]() { OnExportToJsonClicked(); })
					),
					NAME_None,
					LOCTEXT("ExportJsonBtn", "Export JSON"),
					LOCTEXT("ExportJsonBtnTip", "Export this item definition to a JSON file"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Save")
				);

				ToolbarBuilder.AddToolBarButton(
					FUIAction(
						FExecuteAction::CreateLambda([this]() { OnImportFromJsonClicked(); })
					),
					NAME_None,
					LOCTEXT("ImportJsonBtn", "Import JSON"),
					LOCTEXT("ImportJsonBtnTip", "Import item definition from a JSON file, replacing all fragments"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.OpenInExternalEditor")
				);
			}
			ToolbarBuilder.EndSection();
		})
	);

	AddToolbarExtender(ToolbarExtender);
}

// ---------- Fragment Selection ----------

void FArcAssetEditor_ItemDefinition::OnFragmentSelected(
	const UScriptStruct* SelectedStruct,
	EArcFragmentSetType SetType)
{
	if (!SelectedStruct)
	{
		DetailsSwitcher->SetActiveWidgetIndex(0);
		return;
	}

	UArcItemDefinition* ItemDef = GetEditingItemDefinition();
	if (!ItemDef) return;

	// Find the fragment in the appropriate set
	const TSet<FArcInstancedStruct>* Set = nullptr;
	switch (SetType)
	{
	case EArcFragmentSetType::Runtime:
		Set = &ItemDef->GetFragmentSet();
		break;
	case EArcFragmentSetType::ScalableFloat:
		Set = &ItemDef->GetScalableFloatFragments();
		break;
	case EArcFragmentSetType::Editor:
		{
			FProperty* Prop = UArcItemDefinition::StaticClass()->FindPropertyByName(TEXT("EditorFragmentSet"));
			Set = Prop ? Prop->ContainerPtrToValuePtr<TSet<FArcInstancedStruct>>(ItemDef) : nullptr;
		}
		break;
	}

	if (!Set) return;

	FName StructName = SelectedStruct->GetFName();
	const FArcInstancedStruct* Found = Set->FindByHash(GetTypeHash(StructName), StructName);
	if (!Found || !Found->IsValid()) return;

	// We need mutable access to the instanced struct memory for the details view.
	// const_cast is safe here: we are in the editor, editing this object.
	FArcInstancedStruct* MutableFound = const_cast<FArcInstancedStruct*>(Found);
	TSharedPtr<FStructOnScope> StructData = MakeShared<FStructOnScope>(
		MutableFound->InstancedStruct.GetScriptStruct(),
		MutableFound->InstancedStruct.GetMutableMemory());

	FragmentDetailsView->SetStructureData(StructData);
	DetailsSwitcher->SetActiveWidgetIndex(1);
}

void FArcAssetEditor_ItemDefinition::OnFragmentPropertyChanged(const FPropertyChangedEvent& Event)
{
	if (UArcItemDefinition* ItemDef = GetEditingItemDefinition())
	{
		ItemDef->MarkPackageDirty();
	}
}

// ---------- Helpers ----------

UArcItemDefinition* FArcAssetEditor_ItemDefinition::GetEditingItemDefinition() const
{
	const TArray<UObject*>& Objects = GetEditingObjects();
	return Objects.Num() > 0 ? Cast<UArcItemDefinition>(Objects[0]) : nullptr;
}

FText FArcAssetEditor_ItemDefinition::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Item Definition Editor");
}

FText FArcAssetEditor_ItemDefinition::GetToolkitName() const
{
	if (UArcItemDefinition* ItemDef = GetEditingItemDefinition())
	{
		return FText::FromString(ItemDef->GetName());
	}
	return GetBaseToolkitName();
}

FLinearColor FArcAssetEditor_ItemDefinition::GetWorldCentricTabColorScale() const
{
	return FLinearColor(1.0f, 0.6f, 0.0f, 0.5f); // Orange tint
}

FString FArcAssetEditor_ItemDefinition::GetWorldCentricTabPrefix() const
{
	return TEXT("ItemDef ");
}

// ---------- Template Operations ----------

FText FArcAssetEditor_ItemDefinition::GetTemplateNameText() const
{
	if (UArcItemDefinition* ItemDef = GetEditingItemDefinition())
	{
		if (UArcItemDefinitionTemplate* Template = ItemDef->GetSourceTemplate())
		{
			return FText::FromString(Template->GetName());
		}
	}
	return LOCTEXT("NoTemplate", "None");
}

bool FArcAssetEditor_ItemDefinition::HasSourceTemplate() const
{
	if (UArcItemDefinition* ItemDef = GetEditingItemDefinition())
	{
		return ItemDef->GetSourceTemplate() != nullptr;
	}
	return false;
}

void FArcAssetEditor_ItemDefinition::OnChangeTemplateClicked()
{
	UArcItemDefinition* ItemDef = GetEditingItemDefinition();
	if (!ItemDef) return;

	TArray<UArcItemDefinition*> Items;
	Items.Add(ItemDef);

	UArcItemDefinitionTemplate* SelectedTemplate = nullptr;
	const bool bPressedOk = FArcStaticItemHelpers::PickItemSourceTemplateWithPreview(
		SelectedTemplate, Items, EArcMergeMode::SetNewOrReplace);

	if (bPressedOk && SelectedTemplate)
	{
		FScopedTransaction Transaction(LOCTEXT("ChangeTemplateTransaction", "Change Item Template"));
		ItemDef->Modify();
		SelectedTemplate->SetNewOrReplaceItemTemplate(ItemDef);
		ItemDef->MarkPackageDirty();
		RefreshAfterTemplateChange();
	}
}

void FArcAssetEditor_ItemDefinition::OnUpdateFromTemplateClicked()
{
	UArcItemDefinition* ItemDef = GetEditingItemDefinition();
	if (!ItemDef || !ItemDef->GetSourceTemplate()) return;

	TArray<UArcItemDefinition*> Items;
	Items.Add(ItemDef);

	const bool bPressedOk = FArcStaticItemHelpers::PreviewUpdate(
		ItemDef->GetSourceTemplate(), Items);

	if (bPressedOk)
	{
		FScopedTransaction Transaction(LOCTEXT("UpdateFromTemplateTransaction", "Update from Template"));
		ItemDef->Modify();
		ItemDef->GetSourceTemplate()->UpdateFromTemplate(ItemDef);
		ItemDef->MarkPackageDirty();
		RefreshAfterTemplateChange();
	}
}

void FArcAssetEditor_ItemDefinition::OnPushToTemplateClicked()
{
	UArcItemDefinition* ItemDef = GetEditingItemDefinition();
	if (!ItemDef || !ItemDef->GetSourceTemplate()) return;

	UArcItemDefinitionTemplate* Template = ItemDef->GetSourceTemplate();

	// Confirmation dialog
	const FText Message = FText::Format(
		LOCTEXT("PushToTemplateConfirm",
			"This will overwrite the source template '{0}' with the current item's fragments.\n\n"
			"Other items using this template may be affected.\n\n"
			"Continue?"),
		FText::FromString(Template->GetName()));

	if (FMessageDialog::Open(EAppMsgType::OkCancel, Message) != EAppReturnType::Ok)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("PushToTemplateTransaction", "Push to Template"));
	Template->Modify();
	Template->PushFromItem(ItemDef);
	Template->MarkPackageDirty();
}

void FArcAssetEditor_ItemDefinition::RefreshAfterTemplateChange()
{
	// Clear fragment selection
	OnFragmentSelected(nullptr, EArcFragmentSetType::Runtime);

	// Rebuild fragment tree
	if (FragmentListWidget.IsValid())
	{
		FragmentListWidget->RebuildTree();
	}

	// Refresh general details view
	if (GeneralDetailsView.IsValid())
	{
		GeneralDetailsView->ForceRefresh();
	}
}

// ---------- JSON Import/Export ----------

void FArcAssetEditor_ItemDefinition::OnExportToJsonClicked()
{
	UArcItemDefinition* ItemDef = GetEditingItemDefinition();
	if (!ItemDef) return;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	const FString DefaultFilename = ItemDef->GetName() + TEXT(".json");
	TArray<FString> OutFilenames;
	const bool bOpened = DesktopPlatform->SaveFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		LOCTEXT("ExportJsonTitle", "Export Item Definition to JSON").ToString(),
		FPaths::ProjectDir(),
		DefaultFilename,
		TEXT("JSON Files (*.json)|*.json"),
		0,
		OutFilenames
	);

	if (bOpened && OutFilenames.Num() > 0)
	{
		FString JsonString = FArcItemJsonSerializer::ExportToJsonString(ItemDef);
		if (FFileHelper::SaveStringToFile(JsonString, *OutFilenames[0], FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			FNotificationInfo Info(FText::Format(
				LOCTEXT("ExportJsonSuccess", "Exported to {0}"),
				FText::FromString(FPaths::GetCleanFilename(OutFilenames[0]))));
			Info.ExpireDuration = 3.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
}

void FArcAssetEditor_ItemDefinition::OnImportFromJsonClicked()
{
	UArcItemDefinition* ItemDef = GetEditingItemDefinition();
	if (!ItemDef) return;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	TArray<FString> OutFilenames;
	const bool bOpened = DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		LOCTEXT("ImportJsonTitle", "Import Item Definition from JSON").ToString(),
		FPaths::ProjectDir(),
		TEXT(""),
		TEXT("JSON Files (*.json)|*.json"),
		0,
		OutFilenames
	);

	if (!bOpened || OutFilenames.Num() == 0) return;

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *OutFilenames[0]))
	{
		FNotificationInfo Info(LOCTEXT("ImportJsonReadFail", "Failed to read JSON file"));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ImportJsonTransaction", "Import Item From JSON"));
	ItemDef->Modify();

	FString Error;
	bool bSuccess = FArcItemJsonSerializer::ImportFromJsonString(ItemDef, JsonString, Error);

	if (bSuccess)
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("ImportJsonSuccess", "Imported from {0}"),
			FText::FromString(FPaths::GetCleanFilename(OutFilenames[0]))));
		Info.ExpireDuration = 3.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	else
	{
		FNotificationInfo Info(FText::Format(
			LOCTEXT("ImportJsonWarnings", "Imported with warnings: {0}"),
			FText::FromString(Error)));
		Info.ExpireDuration = 8.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	// Refresh the editor
	RefreshAfterTemplateChange();
}

#undef LOCTEXT_NAMESPACE
