// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSQueryEditor.h"

#include "ArcTQSPreviewSetupDetails.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryReference.h"
#include "ArcTQSQueryReferenceDetails.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SArcTQSPreviewViewport.h"
#include "SArcTQSResultsView.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ArcTQSQueryEditor"

const FName FArcTQSQueryEditor::ViewportTabId(TEXT("ArcTQSQueryEditor_Viewport"));
const FName FArcTQSQueryEditor::DetailsTabId(TEXT("ArcTQSQueryEditor_Details"));
const FName FArcTQSQueryEditor::PreviewTabId(TEXT("ArcTQSQueryEditor_Preview"));
const FName FArcTQSQueryEditor::ResultsTabId(TEXT("ArcTQSQueryEditor_Results"));
const FName FArcTQSQueryEditor::ToolkitFName(TEXT("ArcTQSQueryEditor"));
const FName FArcTQSQueryEditor::AppIdentifier(TEXT("ArcTQSQueryEditorApp"));

TSharedRef<FArcTQSQueryEditor> FArcTQSQueryEditor::CreateEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcTQSQueryDefinition* DefinitionToEdit)
{
	TSharedRef<FArcTQSQueryEditor> NewEditor(new FArcTQSQueryEditor());
	NewEditor->InitQueryEditor(Mode, InitToolkitHost, DefinitionToEdit);
	return NewEditor;
}

void FArcTQSQueryEditor::InitQueryEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcTQSQueryDefinition* DefinitionToEdit)
{
	ResultsView = SNew(SArcTQSResultsView)
		.OnItemSelected(FArcTQSOnResultItemSelected::CreateSP(this, &FArcTQSQueryEditor::HandleResultItemSelected));

	PreviewViewport = SNew(SArcTQSPreviewViewport)
		.Definition(DefinitionToEdit)
		.OnResultChanged(FSimpleDelegate::CreateSP(this, &FArcTQSQueryEditor::HandleResultChanged));
	HandleResultChanged();

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent) -> bool
	{
		const FName PreviewSetupName = GET_MEMBER_NAME_CHECKED(UArcTQSQueryDefinition, PreviewSetup);
		if (PropertyAndParent.Property.GetFName() == PreviewSetupName)
		{
			return false;
		}
		for (const FProperty* Parent : PropertyAndParent.ParentProperties)
		{
			if (Parent && Parent->GetFName() == PreviewSetupName)
			{
				return false;
			}
		}
		return true;
	}));
	DetailsView->SetObject(DefinitionToEdit);

	PreviewDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	PreviewDetailsView->RegisterInstancedCustomPropertyLayout(
		UArcTQSQueryDefinition::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcTQSPreviewSetupDetails::MakeInstance));
	PreviewDetailsView->RegisterInstancedCustomPropertyTypeLayout(
		FArcTQSQueryReference::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FArcTQSQueryReferenceDetails::MakeFixedDefinitionInstance));
	PreviewDetailsView->SetObject(DefinitionToEdit);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("ArcTQSQueryEditor_Layout_v1"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.65f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)
					->SetHideTabWell(true)
					->AddTab(ViewportTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->AddTab(ResultsTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.35f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.55f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.45f)
					->AddTab(PreviewTabId, ETabState::OpenedTab)
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

void FArcTQSQueryEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshDetails();
	}
}

void FArcTQSQueryEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		RefreshDetails();
	}
}

void FArcTQSQueryEditor::RefreshDetails()
{
	if (DetailsView.IsValid())
	{
		DetailsView->ForceRefresh();
	}
	if (PreviewDetailsView.IsValid())
	{
		PreviewDetailsView->ForceRefresh();
	}
}

void FArcTQSQueryEditor::HandleResultChanged()
{
	if (ResultsView.IsValid() && PreviewViewport.IsValid())
	{
		ResultsView->SetResult(PreviewViewport->GetResult(), PreviewViewport->GetFailure());
	}
}

void FArcTQSQueryEditor::HandleResultItemSelected(const int32 ItemIndex)
{
	if (PreviewViewport.IsValid())
	{
		PreviewViewport->SetHighlightedItem(ItemIndex);
	}
}

void FArcTQSQueryEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_QueryEditor", "TQS Query Editor"));

	InTabManager->RegisterTabSpawner(ViewportTabId,
		FOnSpawnTab::CreateSP(this, &FArcTQSQueryEditor::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcTQSQueryEditor::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(PreviewTabId,
		FOnSpawnTab::CreateSP(this, &FArcTQSQueryEditor::SpawnTab_Preview))
		.SetDisplayName(LOCTEXT("PreviewTab", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(ResultsTabId,
		FOnSpawnTab::CreateSP(this, &FArcTQSQueryEditor::SpawnTab_Results))
		.SetDisplayName(LOCTEXT("ResultsTab", "Results"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"));
}

void FArcTQSQueryEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(PreviewTabId);
	InTabManager->UnregisterTabSpawner(ResultsTabId);
}

FName FArcTQSQueryEditor::GetToolkitFName() const
{
	return ToolkitFName;
}

FText FArcTQSQueryEditor::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "TQS Query Editor");
}

FString FArcTQSQueryEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "TQS Query ").ToString();
}

FLinearColor FArcTQSQueryEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.6f, 0.4f, 0.7f, 0.5f);
}

TSharedRef<SDockTab> FArcTQSQueryEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTitle", "Viewport"))
		[
			PreviewViewport.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcTQSQueryEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTitle", "Details"))
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcTQSQueryEditor::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewTitle", "Preview"))
		[
			PreviewDetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcTQSQueryEditor::SpawnTab_Results(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ResultsTitle", "Results"))
		[
			ResultsView.ToSharedRef()
		];
}

#undef LOCTEXT_NAMESPACE
