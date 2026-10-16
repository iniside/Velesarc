// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRecoilPresetEditor.h"

#include "Config/ArcRecoilPreset.h"
#include "Framework/Docking/TabManager.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Preview/ArcRecoilPreviewSettings.h"
#include "Preview/SArcRecoilPatternPanel.h"
#include "PropertyEditorModule.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "ArcRecoilPresetEditor"

const FName FArcRecoilPresetEditor::PresetDetailsTabId(TEXT("ArcRecoilPresetEditor_PresetDetails"));
const FName FArcRecoilPresetEditor::PreviewSettingsTabId(TEXT("ArcRecoilPresetEditor_PreviewSettings"));
const FName FArcRecoilPresetEditor::PatternTabId(TEXT("ArcRecoilPresetEditor_Pattern"));
const FName FArcRecoilPresetEditor::ToolkitFName(TEXT("ArcRecoilPresetEditor"));
const FName FArcRecoilPresetEditor::AppIdentifier(TEXT("ArcRecoilPresetEditorApp"));

namespace ArcRecoilPresetEditorPrivate
{
	TSharedRef<IDetailsView> CreateDetailsView()
	{
		FPropertyEditorModule& PropertyEditorModule =
			FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowObjectLabel = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

		return PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	}

	bool IsRepaintOnlyProperty(const FPropertyChangedEvent& Event)
	{
		const FName PropertyName = Event.GetPropertyName();
		if (Event.GetMemberPropertyName() != PropertyName)
		{
			return false;
		}

		return PropertyName == GET_MEMBER_NAME_CHECKED(UArcRecoilPreviewSettings, bShowCameraPaths)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcRecoilPreviewSettings, bShowAimOffsetCloud)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcRecoilPreviewSettings, bShowSpread)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcRecoilPreviewSettings, bShowClampEllipse)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcRecoilPreviewSettings, PlaybackSpeed)
			|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcRecoilPreviewSettings, bLoop);
	}

	ArcRecoil::Preview::FArcRecoilPreviewQuality DraftQuality()
	{
		ArcRecoil::Preview::FArcRecoilPreviewQuality Quality;
		Quality.MaxSeedCount = ArcRecoil::Preview::DraftSeedCount;
		Quality.MaxSpreadSamples = ArcRecoil::Preview::DraftSpreadSamples;
		return Quality;
	}
}

TSharedRef<FArcRecoilPresetEditor> FArcRecoilPresetEditor::CreateEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcRecoilPreset* PresetToEdit)
{
	TSharedRef<FArcRecoilPresetEditor> NewEditor(new FArcRecoilPresetEditor());
	NewEditor->InitPresetEditor(Mode, InitToolkitHost, PresetToEdit);
	return NewEditor;
}

FArcRecoilPresetEditor::~FArcRecoilPresetEditor()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
}

void FArcRecoilPresetEditor::InitPresetEditor(
	const EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UArcRecoilPreset* PresetToEdit)
{
	EditedPreset = PresetToEdit;

	UArcRecoilPreviewSettings* NewPreviewSettings = NewObject<UArcRecoilPreviewSettings>(GetTransientPackage());
	NewPreviewSettings->SetFlags(RF_Transactional);
	PreviewSettings.Reset(NewPreviewSettings);

	PresetDetailsView = ArcRecoilPresetEditorPrivate::CreateDetailsView();
	PresetDetailsView->SetObject(PresetToEdit);

	PreviewSettingsDetailsView = ArcRecoilPresetEditorPrivate::CreateDetailsView();
	PreviewSettingsDetailsView->SetObject(PreviewSettings.Get());

	PatternPanel = SNew(SArcRecoilPatternPanel)
		.Settings(PreviewSettings.Get());

	Resimulate(ArcRecoil::Preview::FArcRecoilPreviewQuality());

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(
		this, &FArcRecoilPresetEditor::HandleObjectPropertyChanged);

	const TSharedRef<FTabManager::FLayout> Layout = FTabManager::NewLayout(TEXT("ArcRecoilPresetEditor_Layout_v1"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.32f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.55f)
						->SetHideTabWell(true)
						->AddTab(PresetDetailsTabId, ETabState::OpenedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.45f)
						->SetHideTabWell(true)
						->AddTab(PreviewSettingsTabId, ETabState::OpenedTab)
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.68f)
					->SetHideTabWell(true)
					->AddTab(PatternTabId, ETabState::OpenedTab)
				)
			)
		);

	TArray<UObject*> ObjectsToEdit;
	ObjectsToEdit.Add(PresetToEdit);

	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		AppIdentifier,
		Layout,
		true,
		true,
		ObjectsToEdit);
}

void FArcRecoilPresetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu_RecoilPresetEditor", "Recoil Preset Editor"));

	// Skips FSimpleAssetEditor::RegisterTabSpawners: it registers a properties tab whose details view
	// is only built by FSimpleAssetEditor::InitEditor, which this toolkit never calls.
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PresetDetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcRecoilPresetEditor::SpawnTab_PresetDetails))
		.SetDisplayName(LOCTEXT("PresetDetailsTab", "Preset"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(PreviewSettingsTabId,
		FOnSpawnTab::CreateSP(this, &FArcRecoilPresetEditor::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSettingsTab", "Preview Settings"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(PatternTabId,
		FOnSpawnTab::CreateSP(this, &FArcRecoilPresetEditor::SpawnTab_Pattern))
		.SetDisplayName(LOCTEXT("PatternTab", "Pattern"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FArcRecoilPresetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(PresetDetailsTabId);
	InTabManager->UnregisterTabSpawner(PreviewSettingsTabId);
	InTabManager->UnregisterTabSpawner(PatternTabId);
}

TSharedRef<SDockTab> FArcRecoilPresetEditor::SpawnTab_PresetDetails(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PresetDetailsTitle", "Preset"))
		[
			PresetDetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcRecoilPresetEditor::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewSettingsTitle", "Preview Settings"))
		[
			PreviewSettingsDetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcRecoilPresetEditor::SpawnTab_Pattern(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PatternTitle", "Pattern"))
		[
			PatternPanel.ToSharedRef()
		];
}

void FArcRecoilPresetEditor::PostRegenerateMenusAndToolbars()
{
	// Skips FSimpleAssetEditor::PostRegenerateMenusAndToolbars: it reads the private EditingObjects
	// list that only FSimpleAssetEditor::InitEditor fills, which this toolkit never calls.
}

void FArcRecoilPresetEditor::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	if (Object == nullptr || (Object != EditedPreset.Get() && Object != PreviewSettings.Get()))
	{
		return;
	}

	if (Object == PreviewSettings.Get() && ArcRecoilPresetEditorPrivate::IsRepaintOnlyProperty(Event))
	{
		if (PatternPanel.IsValid())
		{
			PatternPanel->RepaintCanvases();
		}
		return;
	}

	Resimulate((Event.ChangeType & EPropertyChangeType::Interactive) != 0
		? ArcRecoilPresetEditorPrivate::DraftQuality()
		: ArcRecoil::Preview::FArcRecoilPreviewQuality());
}

void FArcRecoilPresetEditor::Resimulate(const ArcRecoil::Preview::FArcRecoilPreviewQuality& Quality)
{
	const UArcRecoilPreset* Preset = EditedPreset.Get();
	const UArcRecoilPreviewSettings* Settings = PreviewSettings.Get();
	if (Preset == nullptr || Settings == nullptr)
	{
		return;
	}

	const TSharedRef<ArcRecoil::Preview::FArcRecoilPreviewResult> NewResult =
		MakeShared<ArcRecoil::Preview::FArcRecoilPreviewResult>(
			ArcRecoil::Preview::Simulate(*Preset, *Settings, Quality));
	SimResult = NewResult;

	if (PatternPanel.IsValid())
	{
		PatternPanel->RefreshFromResult(SimResult);
	}
}

FText FArcRecoilPresetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Recoil Preset Editor");
}

FString FArcRecoilPresetEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("RecoilPreset ");
}

FLinearColor FArcRecoilPresetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.86f, 0.47f, 0.16f, 0.5f);
}

#undef LOCTEXT_NAMESPACE
