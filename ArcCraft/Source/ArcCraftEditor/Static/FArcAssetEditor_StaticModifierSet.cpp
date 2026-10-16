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

#include "Static/FArcAssetEditor_StaticModifierSet.h"

#include "ArcCraft/Static/ArcStaticModifierSetData.h"
#include "Static/ArcStaticModifierSet_ColumnSchema.h"
#include "Static/ArcStaticModifierSet_DebugEval.h"
#include "Static/ArcStaticModifierSetDetailCustomization.h"
#include "Shared/ArcCraftRecordToolkit.h"
#include "Widgets/SArcModifierTreeView.h"
#include "Widgets/SArcModifierDebugToolbar.h"
#include "Widgets/SArcModifierResultsPanel.h"
#include "Widgets/ArcModifierTreeItem.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "Widgets/ArcCraftRowDetailsProxies.h"
#include "Customizations/ArcCraftRowProxyDetailsCustomization.h"

#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "ArcCraftEditorDebugSettings.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FArcAssetEditor_StaticModifierSet"

const FName FArcAssetEditor_StaticModifierSet::DetailsTabId(TEXT("StaticModifierSetEditor_Details"));
const FName FArcAssetEditor_StaticModifierSet::ModifierTableTabId(TEXT("StaticModifierSetEditor_Table"));
const FName FArcAssetEditor_StaticModifierSet::DebugTabId(TEXT("StaticModifierSetEditor_Debug"));
const FName FArcAssetEditor_StaticModifierSet::ToolkitFName(TEXT("ArcStaticModifierSetEditor"));
const FName FArcAssetEditor_StaticModifierSet::AppIdentifier(TEXT("ArcStaticModifierSetEditorApp"));

// ---------------------------------------------------------------------------
// Row builder for SArcModifierTreeView
// ---------------------------------------------------------------------------

static TArray<TSharedPtr<FArcModifierTreeItem>> BuildStaticModifierSetRows(
	UArcJsonRecordEditObject* Record,
	const FArcModifierDebugContext& /*DebugCtx*/)
{
	TArray<TSharedPtr<FArcModifierTreeItem>> Rows;
	const FArcStaticModifierSetData* ModifierSet = ArcCraftRecordToolkit::GetData<FArcStaticModifierSetData>(Record);
	if (ModifierSet == nullptr)
	{
		return Rows;
	}

	for (int32 i = 0; i < ModifierSet->Modifiers.Num(); ++i)
	{
		TSharedPtr<FArcModifierTreeItem> Item = MakeShared<FArcModifierTreeItem>();
		Item->Kind = EArcModifierRowKind::Modifier;
		Item->ParentIndex = INDEX_NONE;
		Item->ChildIndex = i;
		Rows.Add(Item);
	}

	return Rows;
}

// ---------------------------------------------------------------------------
// Open / lifetime
// ---------------------------------------------------------------------------

bool FArcAssetEditor_StaticModifierSet::OpenRecord(const FString& Filename)
{
	if (ArcCraftRecordToolkit::FocusOpenEditor(Filename))
	{
		return true;
	}
	UArcJsonRecordEditObject* EditRecord = ArcJsonRecordEditing::AcquireRecord(Filename);
	if (EditRecord == nullptr)
	{
		return false;
	}
	if (ArcCraftRecordToolkit::GetData<FArcStaticModifierSetData>(EditRecord) == nullptr)
	{
		ArcJsonRecordEditing::ReleaseRecord(EditRecord);
		return false;
	}
	TSharedRef<FArcAssetEditor_StaticModifierSet> NewEditor(new FArcAssetEditor_StaticModifierSet());
	NewEditor->InitEditor(EditRecord);
	return true;
}

FArcAssetEditor_StaticModifierSet::~FArcAssetEditor_StaticModifierSet()
{
	if (DebugToolbar.IsValid())
	{
		UArcCraftEditorDebugSettings::SaveDebugContext(RecordId, DebugToolbar->GetContext());
	}
	ReleaseRecord();
}

void FArcAssetEditor_StaticModifierSet::ReleaseRecord()
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		EditRecord->OnChanged().Remove(RecordChangedHandle);
		ArcJsonRecordEditing::ReleaseRecord(EditRecord);
	}
	RecordChangedHandle.Reset();
	Record.Reset();
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

void FArcAssetEditor_StaticModifierSet::InitEditor(UArcJsonRecordEditObject* InRecord)
{
	Record = InRecord;
	RecordId = ArcCraftRecordToolkit::GetRecordId(*InRecord);
	RecordChangedHandle = InRecord->OnChanged().AddSP(this, &FArcAssetEditor_StaticModifierSet::HandleRecordChanged);

	FPropertyEditorModule& PropModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowObjectLabel = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsArgs.NotifyHook = this;
	DetailsView = PropModule.CreateStructureDetailView(DetailsArgs, FStructureDetailsViewArgs(), nullptr);
	DetailsView->GetDetailsView()->SetGenericLayoutDetailsDelegate(
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcStaticModifierSetDetailCustomization::MakeInstance));
	RefreshDetails();

	// Per-row details view — persistent IDetailsView backed by transient UObject proxy.
	RowDetailsArgs.bAllowSearch = true;
	RowDetailsArgs.bShowOptions = false;
	RowDetailsArgs.bShowObjectLabel = false;
	RowDetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	RowDetailsView = PropModule.CreateDetailView(RowDetailsArgs);
	RowDetailsView->RegisterInstancedCustomPropertyLayout(
		UArcCraftModifierRowProxy::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcCraftRowProxyDetailsCustomization::MakeInstance));
	RowDetailsView->OnFinishedChangingProperties().AddSP(
		this, &FArcAssetEditor_StaticModifierSet::HandleRowPropertyChanged);

	// Construct proxy object — kept alive for the lifetime of this toolkit.
	ModifierProxy = TStrongObjectPtr<UArcCraftModifierRowProxy>(
		NewObject<UArcCraftModifierRowProxy>(GetTransientPackage()));

	// Host widget — assigned once; content managed by ShowRowSelection.
	SAssignNew(RowDetailsHost, SBox)
	[
		RowDetailsView.ToSharedRef()
	];

	// Modifier table widget
	TArray<FArcModifierColumn> ColumnSchema = GetStaticModifierSetColumnSchema();

	SAssignNew(ModifierTableWidget, SArcModifierTreeView)
		.Record(Record)
		.ColumnSchema(ColumnSchema)
		.AddTopLevelLabel(LOCTEXT("AddMod", "+ Add Modifier"))
		.TopLevelKind(EArcModifierRowKind::Modifier)
		.OnBuildRows(FArcModifierBuildRowsFn::CreateStatic(&BuildStaticModifierSetRows))
		.OnEvaluateDebug(FArcModifierDebugEvaluatorFn::CreateLambda(
			[](UArcJsonRecordEditObject* EditRecord, const FArcModifierDebugContext& Ctx) -> FArcModifierDebugEvaluation
			{
				const FArcStaticModifierSetData* ModifierSet = ArcCraftRecordToolkit::GetData<FArcStaticModifierSetData>(EditRecord);
				if (ModifierSet == nullptr) { return FArcModifierDebugEvaluation(); }
				return EvaluateStaticModifierSetForDebug(*ModifierSet, Ctx);
			}))
		.OnAddTopLevel(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_StaticModifierSet::HandleAddModifier))
		.OnDuplicateRow(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_StaticModifierSet::HandleDuplicateRow))
		.OnDeleteRow(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_StaticModifierSet::HandleDeleteRow))
		.OnReorderTopLevel(FArcOnModifierRowMove::CreateSP(this, &FArcAssetEditor_StaticModifierSet::HandleReorderTopLevel))
		.OnRowSelected(FArcOnModifierRowSelected::CreateSP(this, &FArcAssetEditor_StaticModifierSet::HandleRowSelected))
		.OnEvaluationChanged(FArcOnDebugEvaluationChanged::CreateLambda(
			[this](const FArcModifierDebugEvaluation& NewEval)
			{
				if (ResultsPanel.IsValid())
				{
					ResultsPanel->SetEvaluation(NewEval);
				}
			}));

	// Results panel (simulated output, placed in the Debug tab)
	SAssignNew(ResultsPanel, SArcModifierResultsPanel);

	// Debug toolbar
	SAssignNew(DebugToolbar, SArcModifierDebugToolbar)
		.OnDebugContextChanged(
			FArcOnDebugContextChanged::CreateSP(
				this, &FArcAssetEditor_StaticModifierSet::OnDebugContextChanged));

	DebugToolbar->SetDebugContext(UArcCraftEditorDebugSettings::LoadDebugContext(RecordId));

	// Tab layout
	const TSharedRef<FTabManager::FLayout> Layout =
		FTabManager::NewLayout("ArcStaticModifierSetEditor_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(1.0f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)
					->SetHideTabWell(false)
					->AddTab(ModifierTableTabId, ETabState::OpenedTab)
					->AddTab(DebugTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->SetHideTabWell(false)
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

// ---------------------------------------------------------------------------
// Tab spawners
// ---------------------------------------------------------------------------

void FArcAssetEditor_StaticModifierSet::RegisterTabSpawners(
	const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu", "Static Modifier Set Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ModifierTableTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_StaticModifierSet::SpawnTab_ModifierTable))
		.SetDisplayName(LOCTEXT("ModifierTableTab", "Modifiers"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DebugTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_StaticModifierSet::SpawnTab_Debug))
		.SetDisplayName(LOCTEXT("DebugTab", "Debug"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_StaticModifierSet::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FArcAssetEditor_StaticModifierSet::UnregisterTabSpawners(
	const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ModifierTableTabId);
	InTabManager->UnregisterTabSpawner(DebugTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FArcAssetEditor_StaticModifierSet::SpawnTab_Details(
	const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTitle", "Details"))
		[
			DetailsView->GetWidget().ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcAssetEditor_StaticModifierSet::SpawnTab_ModifierTable(
	const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ModifierTableTitle", "Modifiers"))
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			+ SSplitter::Slot()
			.Value(0.6f)
			[
				ModifierTableWidget.ToSharedRef()
			]

			+ SSplitter::Slot()
			.Value(0.4f)
			[
				RowDetailsHost.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FArcAssetEditor_StaticModifierSet::SpawnTab_Debug(
	const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DebugTitle", "Debug"))
		[
			SNew(SVerticalBox)

			// Standard toolbar (R slider, ingredient tags, live preview)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				DebugToolbar.ToSharedRef()
			]

			// Simulated output panel
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ResultsPanel.ToSharedRef()
			]
		];
}

// ---------------------------------------------------------------------------
// Record binding
// ---------------------------------------------------------------------------

FArcStaticModifierSetData* FArcAssetEditor_StaticModifierSet::GetData() const
{
	return ArcCraftRecordToolkit::GetData<FArcStaticModifierSetData>(Record.Get());
}

void FArcAssetEditor_StaticModifierSet::HandleRecordChanged(UArcJsonRecordEditObject* Object, EArcJsonRecordEditChange Change)
{
	switch (Change)
	{
	case EArcJsonRecordEditChange::Edited:
		if (!bApplyingEdit)
		{
			RefreshActiveRow();
			ModifierTableWidget->RefreshRows();
		}
		break;
	// Undo/redo and reload reallocate the record memory: every view re-reads it.
	case EArcJsonRecordEditChange::UndoRedo:
	case EArcJsonRecordEditChange::Reloaded:
		RefreshFromRecord();
		break;
	case EArcJsonRecordEditChange::Saved:
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

void FArcAssetEditor_StaticModifierSet::RefreshFromRecord()
{
	RefreshDetails();
	ModifierTableWidget->RebuildAll();
	ModifierTableWidget->SetDebugContext(DebugToolbar->GetContext());
}

void FArcAssetEditor_StaticModifierSet::RefreshDetails()
{
	UArcJsonRecordEditObject* EditRecord = Record.Get();
	if (EditRecord == nullptr || !EditRecord->GetRecord().IsValid())
	{
		DetailsView->SetStructureData(nullptr);
		return;
	}
	FInstancedStruct& Data = EditRecord->GetMutableRecord();
	DetailsView->SetStructureData(MakeShared<FStructOnScope>(Data.GetScriptStruct(), Data.GetMutableMemory()));
}

void FArcAssetEditor_StaticModifierSet::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		EditRecord->Modify();
	}
}

void FArcAssetEditor_StaticModifierSet::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		EditRecord->PostEditChange();
	}
}

void FArcAssetEditor_StaticModifierSet::OnDebugContextChanged(
	const FArcModifierDebugContext& NewContext)
{
	if (ModifierTableWidget.IsValid())
	{
		ModifierTableWidget->SetDebugContext(NewContext);
	}

	UArcCraftEditorDebugSettings::SaveDebugContext(RecordId, NewContext);
}

// ---------------------------------------------------------------------------
// ShowRowSelection
// ---------------------------------------------------------------------------

void FArcAssetEditor_StaticModifierSet::ShowRowSelection(TSharedPtr<FArcModifierTreeItem> Selected)
{
	const FArcStaticModifierSetData* ModifierSet = GetData();
	if (Selected.IsValid()
		&& ModifierSet != nullptr
		&& Selected->Kind == EArcModifierRowKind::Modifier
		&& ModifierSet->Modifiers.IsValidIndex(Selected->ChildIndex))
	{
		ModifierProxy->Data = ModifierSet->Modifiers[Selected->ChildIndex];
		ActiveKind = ERowKind::Modifier;
		ActiveChildIdx = Selected->ChildIndex;
		RowDetailsView->SetObject(ModifierProxy.Get(), true);
		return;
	}

	ActiveKind = ERowKind::None;
	ActiveChildIdx = INDEX_NONE;
	RowDetailsView->SetObject(nullptr);
}

void FArcAssetEditor_StaticModifierSet::RefreshActiveRow()
{
	const FArcStaticModifierSetData* ModifierSet = GetData();
	if (ActiveKind == ERowKind::Modifier
		&& ModifierSet != nullptr
		&& ModifierSet->Modifiers.IsValidIndex(ActiveChildIdx))
	{
		ModifierProxy->Data = ModifierSet->Modifiers[ActiveChildIdx];
		RowDetailsView->ForceRefresh();
	}
}

// ---------------------------------------------------------------------------
// Action handlers
// ---------------------------------------------------------------------------

void FArcAssetEditor_StaticModifierSet::HandleAddModifier(TSharedPtr<FArcModifierTreeItem> /*Selected*/)
{
	if (GetData() == nullptr)
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("AddModifier", "Add Modifier"), [this]()
		{
			GetData()->Modifiers.Emplace(FInstancedStruct::Make<FArcCraftModifier_Stats>());
		});
	}

	const int32 NewModIdx = GetData()->Modifiers.Num() - 1;
	ModifierTableWidget->RebuildAll();
	ModifierTableWidget->SelectByKey(EArcModifierRowKind::Modifier, INDEX_NONE, NewModIdx);
}

void FArcAssetEditor_StaticModifierSet::HandleDuplicateRow(TSharedPtr<FArcModifierTreeItem> Selected)
{
	if (!Selected.IsValid() || Selected->Kind != EArcModifierRowKind::Modifier)
	{
		return;
	}

	const FArcStaticModifierSetData* ModifierSet = GetData();
	const int32 ChildIndex = Selected->ChildIndex;
	if (ModifierSet == nullptr || !ModifierSet->Modifiers.IsValidIndex(ChildIndex))
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("DuplicateRow", "Duplicate Modifier"), [this, ChildIndex]()
		{
			TArray<FInstancedStruct>& Modifiers = GetData()->Modifiers;
			FInstancedStruct Copy = Modifiers[ChildIndex];
			Modifiers.Insert(MoveTemp(Copy), ChildIndex + 1);
		});
	}

	ModifierTableWidget->RebuildAll();
}

void FArcAssetEditor_StaticModifierSet::HandleDeleteRow(TSharedPtr<FArcModifierTreeItem> Selected)
{
	if (!Selected.IsValid() || Selected->Kind != EArcModifierRowKind::Modifier)
	{
		return;
	}

	const FArcStaticModifierSetData* ModifierSet = GetData();
	const int32 ChildIndex = Selected->ChildIndex;
	if (ModifierSet == nullptr || !ModifierSet->Modifiers.IsValidIndex(ChildIndex))
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("DeleteRow", "Delete Modifier"), [this, ChildIndex]()
		{
			GetData()->Modifiers.RemoveAt(ChildIndex);
		});
	}

	ModifierTableWidget->RebuildAll();
}

void FArcAssetEditor_StaticModifierSet::HandleReorderTopLevel(int32 From, int32 To)
{
	const FArcStaticModifierSetData* ModifierSet = GetData();
	if (ModifierSet == nullptr)
	{
		return;
	}

	const int32 Count = ModifierSet->Modifiers.Num();
	if (Count == 0 || From == To)
	{
		return;
	}

	To = FMath::Clamp(To, 0, Count - 1);
	if (From == To || !ModifierSet->Modifiers.IsValidIndex(From))
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("ReorderModifier", "Reorder Modifier"), [this, From, To]()
		{
			TArray<FInstancedStruct>& Modifiers = GetData()->Modifiers;
			FInstancedStruct Tmp = MoveTemp(Modifiers[From]);
			Modifiers.RemoveAt(From);
			Modifiers.Insert(MoveTemp(Tmp), To);
		});
	}

	ModifierTableWidget->RebuildAll();
}

void FArcAssetEditor_StaticModifierSet::HandleRowSelected(TSharedPtr<FArcModifierTreeItem> Selected)
{
	ShowRowSelection(Selected);
}

void FArcAssetEditor_StaticModifierSet::HandleRowPropertyChanged(const FPropertyChangedEvent& Event)
{
	if ((Event.ChangeType & EPropertyChangeType::Interactive) != 0)
	{
		return;
	}

	const FArcStaticModifierSetData* ModifierSet = GetData();
	if (ModifierSet == nullptr
		|| ActiveKind != ERowKind::Modifier
		|| !ModifierSet->Modifiers.IsValidIndex(ActiveChildIdx))
	{
		return;
	}

	TGuardValue<bool> Guard(bApplyingEdit, true);
	const int32 ChildIndex = ActiveChildIdx;
	ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("EditModifierRow", "Edit Modifier"), [this, ChildIndex]()
	{
		GetData()->Modifiers[ChildIndex] = ModifierProxy->Data;
	});
}

// ---------------------------------------------------------------------------
// Save / close
// ---------------------------------------------------------------------------

bool FArcAssetEditor_StaticModifierSet::CanSaveAsset() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr && EditRecord->IsOpen();
}

void FArcAssetEditor_StaticModifierSet::SaveAsset_Execute()
{
	ArcCraftRecordToolkit::Save(Record.Get());
}

void FArcAssetEditor_StaticModifierSet::FindInContentBrowser_Execute()
{
	if (const UArcJsonRecordEditObject* Object = Record.Get())
	{
		ArcJsonRecordEditing::BrowseToRecordFile(Object->GetFilename());
	}
}

bool FArcAssetEditor_StaticModifierSet::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	return ArcCraftRecordToolkit::ConfirmClose(Record.Get(), DiscardOnCloseFrame);
}

void FArcAssetEditor_StaticModifierSet::OnClose()
{
	bClosing = true;
	ArcCraftRecordToolkit::CommitClose(Record.Get(), DiscardOnCloseFrame);
	ReleaseRecord();
}

// ---------------------------------------------------------------------------
// Toolkit metadata
// ---------------------------------------------------------------------------

FText FArcAssetEditor_StaticModifierSet::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Static Modifier Set Editor");
}

FText FArcAssetEditor_StaticModifierSet::GetToolkitName() const
{
	const FText Name = ArcCraftRecordToolkit::GetRecordName(Record.Get());
	return Name.IsEmpty() ? GetBaseToolkitName() : Name;
}

FText FArcAssetEditor_StaticModifierSet::GetTabSuffix() const
{
	return ArcCraftRecordToolkit::GetDirtySuffix(Record.Get());
}

FText FArcAssetEditor_StaticModifierSet::GetToolkitToolTipText() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr ? FText::FromString(EditRecord->GetFilename()) : GetBaseToolkitName();
}

FLinearColor FArcAssetEditor_StaticModifierSet::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.57f, 0.28f, 0.75f, 0.5f); // Purple to match asset color
}

FString FArcAssetEditor_StaticModifierSet::GetWorldCentricTabPrefix() const
{
	return TEXT("ModifierSet ");
}

#undef LOCTEXT_NAMESPACE
