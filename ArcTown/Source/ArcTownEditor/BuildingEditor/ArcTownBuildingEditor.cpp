// Copyright Lukasz Baran. All Rights Reserved.

#include "BuildingEditor/ArcTownBuildingEditor.h"

#include "BuildingEditor/SArcTownBuildingPreviewViewport.h"
#include "Catalog/ArcTownBuildingDefinition.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ArcTownBuildingEditor"

const FName FArcTownBuildingEditor::ViewportTabId(TEXT("ArcTownBuildingEditor_Viewport"));
const FName FArcTownBuildingEditor::DetailsTabId(TEXT("ArcTownBuildingEditor_Details"));
const FName FArcTownBuildingEditor::ToolkitFName(TEXT("ArcTownBuildingEditor"));
const FName FArcTownBuildingEditor::AppIdentifier(TEXT("ArcTownBuildingEditorApp"));

TSharedRef<FArcTownBuildingEditor> FArcTownBuildingEditor::CreateEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcTownBuildingDefinition* DefinitionToEdit)
{
	TSharedRef<FArcTownBuildingEditor> NewEditor(new FArcTownBuildingEditor());
	NewEditor->InitBuildingEditor(Mode, InitToolkitHost, DefinitionToEdit);
	return NewEditor;
}

void FArcTownBuildingEditor::InitBuildingEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcTownBuildingDefinition* DefinitionToEdit)
{
	PreviewViewport = SNew(SArcTownBuildingPreviewViewport)
		.Definition(DefinitionToEdit);

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(DefinitionToEdit);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("ArcTownBuildingEditor_Layout_v1"))
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
					->SetSizeCoefficient(0.68f)
					->SetHideTabWell(true)
					->AddTab(ViewportTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.32f)
					->SetHideTabWell(true)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
			)
		);

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(DefinitionToEdit);

	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		AppIdentifier,
		Layout,
		true,
		true,
		ObjectsToEdit);

	if (GEditor != nullptr)
	{
		GEditor->RegisterForUndo(this);
	}
}

void FArcTownBuildingEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshAfterUndoRedo();
	}
}

void FArcTownBuildingEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshAfterUndoRedo();
	}
}

void FArcTownBuildingEditor::RefreshAfterUndoRedo()
{
	if (PreviewViewport.IsValid())
	{
		PreviewViewport->RebuildPreview();
	}
}

void FArcTownBuildingEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_BuildingEditor", "Town Building Editor"));

	InTabManager->RegisterTabSpawner(ViewportTabId,
		FOnSpawnTab::CreateSP(this, &FArcTownBuildingEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcTownBuildingEditor::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FArcTownBuildingEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

FName FArcTownBuildingEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FArcTownBuildingEditor::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Town Building Editor");
}

FString FArcTownBuildingEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Town Building ").ToString();
}

FLinearColor FArcTownBuildingEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.5f, 0.4f, 0.2f, 0.5f);
}

TSharedRef<SDockTab> FArcTownBuildingEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTitle", "Preview"))
		[
			PreviewViewport.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcTownBuildingEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTitle", "Details"))
		[
			DetailsView.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
