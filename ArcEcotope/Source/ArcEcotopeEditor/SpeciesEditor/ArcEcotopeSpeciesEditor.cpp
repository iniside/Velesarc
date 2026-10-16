// Copyright Lukasz Baran. All Rights Reserved.

#include "SpeciesEditor/ArcEcotopeSpeciesEditor.h"

#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "SpeciesEditor/SArcEcotopeBandDetails.h"
#include "SpeciesEditor/SArcEcotopeBandList.h"
#include "SpeciesEditor/SArcEcotopeBandPreviewViewport.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ArcEcotopeSpeciesEditor"

const FName FArcEcotopeSpeciesEditor::BandListTabId(TEXT("ArcEcotopeSpeciesEditor_BandList"));
const FName FArcEcotopeSpeciesEditor::ViewportTabId(TEXT("ArcEcotopeSpeciesEditor_Viewport"));
const FName FArcEcotopeSpeciesEditor::BandDetailsTabId(TEXT("ArcEcotopeSpeciesEditor_BandDetails"));
const FName FArcEcotopeSpeciesEditor::ToolkitFName(TEXT("ArcEcotopeSpeciesEditor"));
const FName FArcEcotopeSpeciesEditor::AppIdentifier(TEXT("ArcEcotopeSpeciesEditorApp"));

FArcEcotopeSpeciesEditor::FArcEcotopeSpeciesEditor()
	: Selection(MakeShared<FArcEcotopeBandSelection>())
{
}

TSharedRef<FArcEcotopeSpeciesEditor> FArcEcotopeSpeciesEditor::CreateEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcEcotopeSpeciesModel* SpeciesToEdit)
{
	TSharedRef<FArcEcotopeSpeciesEditor> NewEditor(new FArcEcotopeSpeciesEditor());
	NewEditor->InitSpeciesEditor(Mode, InitToolkitHost, SpeciesToEdit);
	return NewEditor;
}

void FArcEcotopeSpeciesEditor::InitSpeciesEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcEcotopeSpeciesModel* SpeciesToEdit)
{
	BandList = SNew(SArcEcotopeBandList)
		.Species(SpeciesToEdit)
		.Selection(Selection);

	PreviewViewport = SNew(SArcEcotopeBandPreviewViewport)
		.Species(SpeciesToEdit)
		.Selection(Selection);

	BandDetails = SNew(SArcEcotopeBandDetails)
		.Species(SpeciesToEdit)
		.Selection(Selection);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("ArcEcotopeSpeciesEditor_Layout_v1"))
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
					->SetSizeCoefficient(0.22f)
					->SetHideTabWell(true)
					->AddTab(BandListTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.50f)
					->SetHideTabWell(true)
					->AddTab(ViewportTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.28f)
					->SetHideTabWell(true)
					->AddTab(BandDetailsTabId, ETabState::OpenedTab)
				)
			)
		);

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(SpeciesToEdit);

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

void FArcEcotopeSpeciesEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshAfterUndoRedo();
	}
}

void FArcEcotopeSpeciesEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshAfterUndoRedo();
	}
}

void FArcEcotopeSpeciesEditor::RefreshAfterUndoRedo()
{
	const bool bSelectionBroadcast = BandList.IsValid() && BandList->RefreshAfterUndoRedo();

	if (!bSelectionBroadcast && PreviewViewport.IsValid())
	{
		PreviewViewport->RebuildPreview();
	}
}

void FArcEcotopeSpeciesEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_SpeciesEditor", "Ecotope Species Editor"));

	InTabManager->RegisterTabSpawner(BandListTabId,
		FOnSpawnTab::CreateSP(this, &FArcEcotopeSpeciesEditor::SpawnTab_BandList))
		.SetDisplayName(LOCTEXT("BandListTab", "Bands"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(ViewportTabId,
		FOnSpawnTab::CreateSP(this, &FArcEcotopeSpeciesEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(BandDetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcEcotopeSpeciesEditor::SpawnTab_BandDetails))
		.SetDisplayName(LOCTEXT("BandDetailsTab", "Band Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FArcEcotopeSpeciesEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(BandListTabId);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(BandDetailsTabId);
}

FName FArcEcotopeSpeciesEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FArcEcotopeSpeciesEditor::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Ecotope Species Editor");
}

FString FArcEcotopeSpeciesEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Ecotope Species ").ToString();
}

FLinearColor FArcEcotopeSpeciesEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.5f, 0.3f, 0.5f);
}

TSharedRef<SDockTab> FArcEcotopeSpeciesEditor::SpawnTab_BandList(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("BandListTitle", "Bands"))
		[
			BandList.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcEcotopeSpeciesEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTitle", "Preview"))
		[
			PreviewViewport.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcEcotopeSpeciesEditor::SpawnTab_BandDetails(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("BandDetailsTitle", "Band Details"))
		[
			BandDetails.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
