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

#include "RandomPool/FArcAssetEditor_RandomPoolDefinition.h"

#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/Recipe/ArcRandomPoolEvaluation.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "RandomPool/ArcRandomPool_ColumnSchema.h"
#include "RandomPool/ArcRandomPool_DebugEval.h"
#include "RandomPool/ArcRandomPoolDefinitionDetailCustomization.h"
#include "Shared/ArcCraftRecordToolkit.h"
#include "Widgets/SArcModifierTreeView.h"
#include "Widgets/SArcModifierDebugToolbar.h"
#include "Widgets/SArcModifierResultsPanel.h"
#include "Widgets/ArcModifierTreeItem.h"
#include "Widgets/ArcModifierDebugContext.h"
#include "Widgets/ArcCraftRowDetailsProxies.h"
#include "Customizations/ArcCraftRowProxyDetailsCustomization.h"

#include "ArcCraftEditorDebugSettings.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "StructUtils/InstancedStruct.h"
#include "Modules/ModuleManager.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "UObject/UObjectIterator.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

#define LOCTEXT_NAMESPACE "FArcAssetEditor_RandomPoolDefinition"

namespace ArcRandomPoolEditor_Impl
{
	static const double TintDurationSeconds = 2.0;
} // namespace ArcRandomPoolEditor_Impl

const FName FArcAssetEditor_RandomPoolDefinition::DetailsTabId(TEXT("RandomPoolEditor_Details"));
const FName FArcAssetEditor_RandomPoolDefinition::EntryTableTabId(TEXT("RandomPoolEditor_Table"));
const FName FArcAssetEditor_RandomPoolDefinition::DebugTabId(TEXT("RandomPoolEditor_Debug"));
const FName FArcAssetEditor_RandomPoolDefinition::ToolkitFName(TEXT("ArcRandomPoolEditor"));
const FName FArcAssetEditor_RandomPoolDefinition::AppIdentifier(TEXT("ArcRandomPoolEditorApp"));

// ---------------------------------------------------------------------------
// Row builder for SArcModifierTreeView
// ---------------------------------------------------------------------------

static TArray<TSharedPtr<FArcModifierTreeItem>> BuildRandomPoolRows(
	UArcJsonRecordEditObject* Record,
	const FArcModifierDebugContext& /*DebugCtx*/)
{
	TArray<TSharedPtr<FArcModifierTreeItem>> Rows;

	const FArcRandomPoolDefinitionData* PoolDef = ArcCraftRecordToolkit::GetData<FArcRandomPoolDefinitionData>(Record);
	if (PoolDef == nullptr)
	{
		return Rows;
	}

	for (int32 EntryIdx = 0; EntryIdx < PoolDef->Entries.Num(); ++EntryIdx)
	{
		TSharedPtr<FArcModifierTreeItem> EntryItem = MakeShared<FArcModifierTreeItem>();
		EntryItem->Kind = EArcModifierRowKind::Entry;
		EntryItem->ParentIndex = INDEX_NONE;
		EntryItem->ChildIndex = EntryIdx;

		// Build child modifier rows
		const FArcRandomPoolEntry& Entry = PoolDef->Entries[EntryIdx];
		for (int32 ModIdx = 0; ModIdx < Entry.Modifiers.Num(); ++ModIdx)
		{
			TSharedPtr<FArcModifierTreeItem> ModItem = MakeShared<FArcModifierTreeItem>();
			ModItem->Kind = EArcModifierRowKind::Modifier;
			ModItem->ParentIndex = EntryIdx;
			ModItem->ChildIndex = ModIdx;
			EntryItem->Children.Add(ModItem);
		}

		Rows.Add(EntryItem);
	}

	return Rows;
}

// ---------------------------------------------------------------------------
// Open / lifetime
// ---------------------------------------------------------------------------

bool FArcAssetEditor_RandomPoolDefinition::OpenRecord(const FString& Filename)
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
	if (ArcCraftRecordToolkit::GetData<FArcRandomPoolDefinitionData>(EditRecord) == nullptr)
	{
		ArcJsonRecordEditing::ReleaseRecord(EditRecord);
		return false;
	}
	TSharedRef<FArcAssetEditor_RandomPoolDefinition> NewEditor(new FArcAssetEditor_RandomPoolDefinition());
	NewEditor->InitEditor(EditRecord);
	return true;
}

FArcAssetEditor_RandomPoolDefinition::~FArcAssetEditor_RandomPoolDefinition()
{
	if (DebugToolbar.IsValid())
	{
		UArcCraftEditorDebugSettings::SaveDebugContext(RecordId, DebugToolbar->GetContext());
	}
	ReleaseRecord();
}

void FArcAssetEditor_RandomPoolDefinition::ReleaseRecord()
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

void FArcAssetEditor_RandomPoolDefinition::InitEditor(UArcJsonRecordEditObject* InRecord)
{
	Record = InRecord;
	RecordId = ArcCraftRecordToolkit::GetRecordId(*InRecord);
	RecordChangedHandle = InRecord->OnChanged().AddSP(this, &FArcAssetEditor_RandomPoolDefinition::HandleRecordChanged);

	// Default debug selection mode: SimpleRandom with MaxSelections=1
	DebugSelectionMode.InitializeAs<FArcRandomPoolSelection_SimpleRandom>();

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
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcRandomPoolDefinitionDetailCustomization::MakeInstance));
	RefreshDetails();

	// Per-row details view — persistent IDetailsView backed by transient UObject proxies.
	RowDetailsArgs.bAllowSearch = true;
	RowDetailsArgs.bShowOptions = false;
	RowDetailsArgs.bShowObjectLabel = false;
	RowDetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	RowDetailsView = PropModule.CreateDetailView(RowDetailsArgs);
	RowDetailsView->RegisterInstancedCustomPropertyLayout(
		UArcCraftPoolEntryRowProxy::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcCraftRowProxyDetailsCustomization::MakeInstance));
	RowDetailsView->RegisterInstancedCustomPropertyLayout(
		UArcCraftModifierRowProxy::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcCraftRowProxyDetailsCustomization::MakeInstance));
	RowDetailsView->OnFinishedChangingProperties().AddSP(
		this, &FArcAssetEditor_RandomPoolDefinition::HandleRowPropertyChanged);

	// Construct proxy objects — kept alive for the lifetime of this toolkit.
	EntryProxy = TStrongObjectPtr<UArcCraftPoolEntryRowProxy>(
		NewObject<UArcCraftPoolEntryRowProxy>(GetTransientPackage()));
	ModifierProxy = TStrongObjectPtr<UArcCraftModifierRowProxy>(
		NewObject<UArcCraftModifierRowProxy>(GetTransientPackage()));

	// Host widget — assigned once; content managed by ShowRowSelection.
	SAssignNew(RowDetailsHost, SBox)
	[
		RowDetailsView.ToSharedRef()
	];

	// Entry table widget
	TArray<FArcModifierColumn> EntrySchema = ArcRandomPool_ColumnSchema::GetEntryRowSchema();
	TArray<FArcModifierColumn> ModifierSchema = ArcRandomPool_ColumnSchema::GetModifierChildRowSchema();

	// Merge both schemas into one array for SArcModifierTreeView
	TArray<FArcModifierColumn> CombinedSchema;
	CombinedSchema.Append(MoveTemp(EntrySchema));
	CombinedSchema.Append(MoveTemp(ModifierSchema));

	SAssignNew(EntryTableWidget, SArcModifierTreeView)
		.Record(Record)
		.ColumnSchema(CombinedSchema)
		.AddTopLevelLabel(LOCTEXT("AddEntry", "+ Add Entry"))
		.TopLevelKind(EArcModifierRowKind::Entry)
		.OnBuildRows(FArcModifierBuildRowsFn::CreateStatic(&BuildRandomPoolRows))
		.OnEvaluateDebug(FArcModifierDebugEvaluatorFn::CreateLambda(
			[](UArcJsonRecordEditObject* EditRecord, const FArcModifierDebugContext& Ctx) -> FArcModifierDebugEvaluation
			{
				const FArcRandomPoolDefinitionData* PoolDef = ArcCraftRecordToolkit::GetData<FArcRandomPoolDefinitionData>(EditRecord);
				if (PoolDef == nullptr) { return FArcModifierDebugEvaluation(); }
				return ArcRandomPool_DebugEval::EvaluateRandomPoolForDebug(*PoolDef, Ctx);
			}))
		.OnAddTopLevel(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::HandleAddEntry))
		.OnAddChildOfSelected(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::HandleAddModifierUnderEntry))
		.OnDuplicateRow(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::HandleDuplicateRow))
		.OnDeleteRow(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::HandleDeleteRow))
		.OnReorderTopLevel(FArcOnModifierRowMove::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::HandleReorderTopLevel))
		.OnRowSelected(FArcOnModifierRowSelected::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::HandleRowSelected))
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
				this, &FArcAssetEditor_RandomPoolDefinition::OnDebugContextChanged));

	DebugToolbar->SetDebugContext(UArcCraftEditorDebugSettings::LoadDebugContext(RecordId));

	// Tab layout
	const TSharedRef<FTabManager::FLayout> Layout =
		FTabManager::NewLayout("ArcRandomPoolEditor_v1")
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
					->AddTab(EntryTableTabId, ETabState::OpenedTab)
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

void FArcAssetEditor_RandomPoolDefinition::RegisterTabSpawners(
	const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu", "Random Pool Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(EntryTableTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::SpawnTab_EntryTable))
		.SetDisplayName(LOCTEXT("EntryTableTab", "Entries"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DebugTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::SpawnTab_Debug))
		.SetDisplayName(LOCTEXT("DebugTab", "Debug"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FArcAssetEditor_RandomPoolDefinition::UnregisterTabSpawners(
	const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(EntryTableTabId);
	InTabManager->UnregisterTabSpawner(DebugTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FArcAssetEditor_RandomPoolDefinition::SpawnTab_Details(
	const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTitle", "Details"))
		[
			DetailsView->GetWidget().ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcAssetEditor_RandomPoolDefinition::SpawnTab_EntryTable(
	const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("EntryTableTitle", "Entries"))
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			+ SSplitter::Slot()
			.Value(0.6f)
			[
				EntryTableWidget.ToSharedRef()
			]

			+ SSplitter::Slot()
			.Value(0.4f)
			[
				RowDetailsHost.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FArcAssetEditor_RandomPoolDefinition::SpawnTab_Debug(
	const FSpawnTabArgs& Args)
{
	TSharedRef<FArcAssetEditor_RandomPoolDefinition> SelfRef =
		StaticCastSharedRef<FArcAssetEditor_RandomPoolDefinition>(AsShared());

	// Populate the selection struct list once per tab spawn.
	AvailableSelectionStructs.Reset();
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		UScriptStruct* Struct = *It;
		if (Struct != nullptr
			&& Struct != FArcRandomPoolSelectionMode::StaticStruct()
			&& Struct->IsChildOf(FArcRandomPoolSelectionMode::StaticStruct()))
		{
			AvailableSelectionStructs.Add(Struct);
		}
	}

	return SNew(SDockTab)
		.Label(LOCTEXT("DebugTitle", "Debug"))
		[
			SNew(SVerticalBox)

			// Standard toolbar (R slider, live preview)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				DebugToolbar.ToSharedRef()
			]

			// SelectionMode picker + Roll button
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(6.0f, 4.0f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 4.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SelectionModeLabel", "Selection Mode (preview):"))
							.Font(FAppStyle::GetFontStyle("SmallFont"))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(SComboBox<UScriptStruct*>)
							.OptionsSource(&SelfRef->AvailableSelectionStructs)
							.OnGenerateWidget_Lambda([](UScriptStruct* Item) -> TSharedRef<SWidget>
							{
								const FText Label = Item
									? Item->GetDisplayNameText()
									: LOCTEXT("NullStruct", "(null)");
								return SNew(STextBlock).Text(Label);
							})
							.OnSelectionChanged_Lambda([SelfRef](UScriptStruct* NewStruct, ESelectInfo::Type) -> void
							{
								if (NewStruct != nullptr)
								{
									SelfRef->DebugSelectionMode.InitializeAs(NewStruct, nullptr);
									if (SelfRef->EntryTableWidget.IsValid())
									{
										SelfRef->EntryTableWidget->RebuildAll();
									}
								}
							})
							.InitiallySelectedItem(const_cast<UScriptStruct*>(SelfRef->DebugSelectionMode.GetScriptStruct()))
							.Content()
							[
								SNew(STextBlock)
								.Text_Lambda([SelfRef]() -> FText
								{
									const UScriptStruct* Struct = SelfRef->DebugSelectionMode.GetScriptStruct();
									return Struct
										? Struct->GetDisplayNameText()
										: LOCTEXT("NoMode", "(none)");
								})
								.Font(FAppStyle::GetFontStyle("SmallFont"))
							]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.Text(LOCTEXT("RollButton", "Roll"))
							.ToolTipText(LOCTEXT("RollTip",
								"Perform one weighted random selection using the current debug context. "
								"Selected entries will be highlighted briefly."))
							.OnClicked_Lambda([SelfRef]() -> FReply
							{
								return SelfRef->OnRollClicked();
							})
						]
					]

					// Note line
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectionModeNote",
							"Selection mode is configured per use site on the consuming recipe modifier "
							"source — this picker is for editor preview only."))
						.Font(FAppStyle::GetFontStyle("SmallFont"))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
						.AutoWrapText(true)
					]
				]
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

FArcRandomPoolDefinitionData* FArcAssetEditor_RandomPoolDefinition::GetData() const
{
	return ArcCraftRecordToolkit::GetData<FArcRandomPoolDefinitionData>(Record.Get());
}

void FArcAssetEditor_RandomPoolDefinition::HandleRecordChanged(UArcJsonRecordEditObject* Object, EArcJsonRecordEditChange Change)
{
	switch (Change)
	{
	case EArcJsonRecordEditChange::Edited:
		if (!bApplyingEdit)
		{
			RefreshActiveRow();
			EntryTableWidget->RefreshRows();
		}
		break;
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

void FArcAssetEditor_RandomPoolDefinition::RefreshFromRecord()
{
	RefreshDetails();
	EntryTableWidget->RebuildAll();
	EntryTableWidget->SetDebugContext(DebugToolbar->GetContext());
}

void FArcAssetEditor_RandomPoolDefinition::RefreshDetails()
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

void FArcAssetEditor_RandomPoolDefinition::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		EditRecord->Modify();
	}
}

void FArcAssetEditor_RandomPoolDefinition::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		EditRecord->PostEditChange();
	}
}

void FArcAssetEditor_RandomPoolDefinition::OnDebugContextChanged(
	const FArcModifierDebugContext& NewContext)
{
	if (EntryTableWidget.IsValid())
	{
		EntryTableWidget->SetDebugContext(NewContext);
	}

	UArcCraftEditorDebugSettings::SaveDebugContext(RecordId, NewContext);
}

// ---------------------------------------------------------------------------
// ShowRowSelection
// ---------------------------------------------------------------------------

void FArcAssetEditor_RandomPoolDefinition::ShowRowSelection(TSharedPtr<FArcModifierTreeItem> Selected)
{
	const FArcRandomPoolDefinitionData* PoolDef = GetData();
	if (Selected.IsValid() && PoolDef != nullptr)
	{
		if (Selected->Kind == EArcModifierRowKind::Entry
			&& PoolDef->Entries.IsValidIndex(Selected->ChildIndex))
		{
			EntryProxy->Data = PoolDef->Entries[Selected->ChildIndex];
			ActiveKind = ERowKind::Entry;
			ActiveParentIdx = INDEX_NONE;
			ActiveChildIdx = Selected->ChildIndex;
			RowDetailsView->SetObject(EntryProxy.Get(), true);
			return;
		}
		if (Selected->Kind == EArcModifierRowKind::Modifier
			&& PoolDef->Entries.IsValidIndex(Selected->ParentIndex)
			&& PoolDef->Entries[Selected->ParentIndex].Modifiers.IsValidIndex(Selected->ChildIndex))
		{
			ModifierProxy->Data = PoolDef->Entries[Selected->ParentIndex].Modifiers[Selected->ChildIndex];
			ActiveKind = ERowKind::Modifier;
			ActiveParentIdx = Selected->ParentIndex;
			ActiveChildIdx = Selected->ChildIndex;
			RowDetailsView->SetObject(ModifierProxy.Get(), true);
			return;
		}
	}

	ActiveKind = ERowKind::None;
	ActiveParentIdx = INDEX_NONE;
	ActiveChildIdx = INDEX_NONE;
	RowDetailsView->SetObject(nullptr);
}

void FArcAssetEditor_RandomPoolDefinition::RefreshActiveRow()
{
	const FArcRandomPoolDefinitionData* PoolDef = GetData();
	if (PoolDef == nullptr)
	{
		return;
	}
	if (ActiveKind == ERowKind::Entry && PoolDef->Entries.IsValidIndex(ActiveChildIdx))
	{
		EntryProxy->Data = PoolDef->Entries[ActiveChildIdx];
		RowDetailsView->ForceRefresh();
	}
	else if (ActiveKind == ERowKind::Modifier
		&& PoolDef->Entries.IsValidIndex(ActiveParentIdx)
		&& PoolDef->Entries[ActiveParentIdx].Modifiers.IsValidIndex(ActiveChildIdx))
	{
		ModifierProxy->Data = PoolDef->Entries[ActiveParentIdx].Modifiers[ActiveChildIdx];
		RowDetailsView->ForceRefresh();
	}
}

// ---------------------------------------------------------------------------
// Action handlers
// ---------------------------------------------------------------------------

void FArcAssetEditor_RandomPoolDefinition::HandleAddEntry(TSharedPtr<FArcModifierTreeItem> /*Selected*/)
{
	if (GetData() == nullptr)
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("AddEntry", "Add Entry"), [this]()
		{
			TArray<FArcRandomPoolEntry>& Entries = GetData()->Entries;
			FArcRandomPoolEntry& NewEntry = Entries.AddDefaulted_GetRef();
			NewEntry.DisplayName = FText::FromString(FString::Printf(TEXT("Entry %d"), Entries.Num()));
		});
	}

	const int32 NewEntryIdx = GetData()->Entries.Num() - 1;
	EntryTableWidget->RebuildAll();
	EntryTableWidget->SelectByKey(EArcModifierRowKind::Entry, INDEX_NONE, NewEntryIdx);
}

void FArcAssetEditor_RandomPoolDefinition::HandleAddModifierUnderEntry(TSharedPtr<FArcModifierTreeItem> Selected)
{
	if (!Selected.IsValid() || Selected->Kind != EArcModifierRowKind::Entry)
	{
		return;
	}

	const FArcRandomPoolDefinitionData* PoolDef = GetData();
	const int32 EntryIdx = Selected->ChildIndex;
	if (PoolDef == nullptr || !PoolDef->Entries.IsValidIndex(EntryIdx))
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("AddModifier", "Add Modifier"), [this, EntryIdx]()
		{
			GetData()->Entries[EntryIdx].Modifiers.Emplace(FInstancedStruct::Make<FArcCraftModifier_Stats>());
		});
	}

	const int32 NewModIdx = GetData()->Entries[EntryIdx].Modifiers.Num() - 1;
	EntryTableWidget->RebuildAll();
	EntryTableWidget->SelectByKey(EArcModifierRowKind::Modifier, EntryIdx, NewModIdx);
}

void FArcAssetEditor_RandomPoolDefinition::HandleDuplicateRow(TSharedPtr<FArcModifierTreeItem> Selected)
{
	const FArcRandomPoolDefinitionData* PoolDef = GetData();
	if (!Selected.IsValid() || PoolDef == nullptr)
	{
		return;
	}

	const EArcModifierRowKind Kind = Selected->Kind;
	const int32 ParentIndex = Selected->ParentIndex;
	const int32 ChildIndex = Selected->ChildIndex;
	const bool bValidEntry = Kind == EArcModifierRowKind::Entry && PoolDef->Entries.IsValidIndex(ChildIndex);
	const bool bValidModifier = Kind == EArcModifierRowKind::Modifier
		&& PoolDef->Entries.IsValidIndex(ParentIndex)
		&& PoolDef->Entries[ParentIndex].Modifiers.IsValidIndex(ChildIndex);
	if (!bValidEntry && !bValidModifier)
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("DuplicateRow", "Duplicate Row"), [this, bValidEntry, ParentIndex, ChildIndex]()
		{
			TArray<FArcRandomPoolEntry>& Entries = GetData()->Entries;
			if (bValidEntry)
			{
				FArcRandomPoolEntry Copy = Entries[ChildIndex];
				Entries.Insert(MoveTemp(Copy), ChildIndex + 1);
			}
			else
			{
				TArray<FInstancedStruct>& Modifiers = Entries[ParentIndex].Modifiers;
				FInstancedStruct Copy = Modifiers[ChildIndex];
				Modifiers.Insert(MoveTemp(Copy), ChildIndex + 1);
			}
		});
	}

	EntryTableWidget->RebuildAll();
}

void FArcAssetEditor_RandomPoolDefinition::HandleDeleteRow(TSharedPtr<FArcModifierTreeItem> Selected)
{
	const FArcRandomPoolDefinitionData* PoolDef = GetData();
	if (!Selected.IsValid() || PoolDef == nullptr)
	{
		return;
	}

	const EArcModifierRowKind Kind = Selected->Kind;
	const int32 ParentIndex = Selected->ParentIndex;
	const int32 ChildIndex = Selected->ChildIndex;
	const bool bValidEntry = Kind == EArcModifierRowKind::Entry && PoolDef->Entries.IsValidIndex(ChildIndex);
	const bool bValidModifier = Kind == EArcModifierRowKind::Modifier
		&& PoolDef->Entries.IsValidIndex(ParentIndex)
		&& PoolDef->Entries[ParentIndex].Modifiers.IsValidIndex(ChildIndex);
	if (!bValidEntry && !bValidModifier)
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("DeleteRow", "Delete Row"), [this, bValidEntry, ParentIndex, ChildIndex]()
		{
			TArray<FArcRandomPoolEntry>& Entries = GetData()->Entries;
			if (bValidEntry)
			{
				Entries.RemoveAt(ChildIndex);
			}
			else
			{
				Entries[ParentIndex].Modifiers.RemoveAt(ChildIndex);
			}
		});
	}

	EntryTableWidget->RebuildAll();
}

void FArcAssetEditor_RandomPoolDefinition::HandleReorderTopLevel(int32 From, int32 To)
{
	const FArcRandomPoolDefinitionData* PoolDef = GetData();
	if (PoolDef == nullptr)
	{
		return;
	}

	const int32 EntryCount = PoolDef->Entries.Num();
	if (EntryCount == 0 || From == To)
	{
		return;
	}

	To = FMath::Clamp(To, 0, EntryCount - 1);
	if (From == To || !PoolDef->Entries.IsValidIndex(From))
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("ReorderEntry", "Reorder Entry"), [this, From, To]()
		{
			TArray<FArcRandomPoolEntry>& Entries = GetData()->Entries;
			FArcRandomPoolEntry Tmp = MoveTemp(Entries[From]);
			Entries.RemoveAt(From);
			Entries.Insert(MoveTemp(Tmp), To);
		});
	}

	EntryTableWidget->RebuildAll();
}

void FArcAssetEditor_RandomPoolDefinition::HandleRowSelected(TSharedPtr<FArcModifierTreeItem> Selected)
{
	ShowRowSelection(Selected);
}

void FArcAssetEditor_RandomPoolDefinition::HandleRowPropertyChanged(const FPropertyChangedEvent& Event)
{
	if ((Event.ChangeType & EPropertyChangeType::Interactive) != 0)
	{
		return;
	}

	const FArcRandomPoolDefinitionData* PoolDef = GetData();
	if (PoolDef == nullptr)
	{
		return;
	}

	const int32 ParentIndex = ActiveParentIdx;
	const int32 ChildIndex = ActiveChildIdx;
	TGuardValue<bool> Guard(bApplyingEdit, true);
	if (ActiveKind == ERowKind::Entry && PoolDef->Entries.IsValidIndex(ChildIndex))
	{
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("EditEntryRow", "Edit Entry"), [this, ChildIndex]()
		{
			GetData()->Entries[ChildIndex] = EntryProxy->Data;
		});
	}
	else if (ActiveKind == ERowKind::Modifier
		&& PoolDef->Entries.IsValidIndex(ParentIndex)
		&& PoolDef->Entries[ParentIndex].Modifiers.IsValidIndex(ChildIndex))
	{
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("EditModifierRow", "Edit Modifier"), [this, ParentIndex, ChildIndex]()
		{
			GetData()->Entries[ParentIndex].Modifiers[ChildIndex] = ModifierProxy->Data;
		});
	}
}

FReply FArcAssetEditor_RandomPoolDefinition::OnRollClicked()
{
	const FArcRandomPoolDefinitionData* PoolDef = GetData();
	if (PoolDef == nullptr)
	{
		return FReply::Handled();
	}

	FArcModifierDebugContext DebugCtx;
	if (DebugToolbar.IsValid())
	{
		DebugCtx = DebugToolbar->GetContext();
	}

	TArray<int32> SelectedIndices = ArcRandomPool_DebugEval::RollOnce(
		*PoolDef, DebugCtx, DebugSelectionMode);

	// Store which indices to tint and set expiry time
	TintedEntryIndices.Reset();
	for (const int32 Idx : SelectedIndices)
	{
		TintedEntryIndices.Add(Idx);
	}
	TintExpiryTime = FPlatformTime::Seconds() + ArcRandomPoolEditor_Impl::TintDurationSeconds;

	// Arm the decay ticker
	if (TintTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TintTickerHandle);
		TintTickerHandle.Reset();
	}

	TintTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateSP(this, &FArcAssetEditor_RandomPoolDefinition::OnTintDecay),
		0.05f); // check every 50ms

	// Refresh table to show tint
	if (EntryTableWidget.IsValid())
	{
		EntryTableWidget->RefreshRows();
	}

	return FReply::Handled();
}

bool FArcAssetEditor_RandomPoolDefinition::OnTintDecay(float /*DeltaTime*/)
{
	if (FPlatformTime::Seconds() >= TintExpiryTime)
	{
		TintedEntryIndices.Reset();
		TintTickerHandle.Reset();

		if (EntryTableWidget.IsValid())
		{
			EntryTableWidget->RefreshRows();
		}

		return false; // Remove ticker
	}
	return true; // Keep ticking
}

// ---------------------------------------------------------------------------
// Save / close
// ---------------------------------------------------------------------------

bool FArcAssetEditor_RandomPoolDefinition::CanSaveAsset() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr && EditRecord->IsOpen();
}

void FArcAssetEditor_RandomPoolDefinition::SaveAsset_Execute()
{
	ArcCraftRecordToolkit::Save(Record.Get());
}

void FArcAssetEditor_RandomPoolDefinition::FindInContentBrowser_Execute()
{
	if (const UArcJsonRecordEditObject* Object = Record.Get())
	{
		ArcJsonRecordEditing::BrowseToRecordFile(Object->GetFilename());
	}
}

bool FArcAssetEditor_RandomPoolDefinition::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	return ArcCraftRecordToolkit::ConfirmClose(Record.Get(), DiscardOnCloseFrame);
}

void FArcAssetEditor_RandomPoolDefinition::OnClose()
{
	bClosing = true;
	ArcCraftRecordToolkit::CommitClose(Record.Get(), DiscardOnCloseFrame);
	ReleaseRecord();
}

// ---------------------------------------------------------------------------
// Toolkit metadata
// ---------------------------------------------------------------------------

FText FArcAssetEditor_RandomPoolDefinition::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Random Pool Editor");
}

FText FArcAssetEditor_RandomPoolDefinition::GetToolkitName() const
{
	const FText Name = ArcCraftRecordToolkit::GetRecordName(Record.Get());
	return Name.IsEmpty() ? GetBaseToolkitName() : Name;
}

FText FArcAssetEditor_RandomPoolDefinition::GetTabSuffix() const
{
	return ArcCraftRecordToolkit::GetDirtySuffix(Record.Get());
}

FText FArcAssetEditor_RandomPoolDefinition::GetToolkitToolTipText() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr ? FText::FromString(EditRecord->GetFilename()) : GetBaseToolkitName();
}

FLinearColor FArcAssetEditor_RandomPoolDefinition::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.57f, 0.28f, 0.75f, 0.5f); // Purple to match asset color
}

FString FArcAssetEditor_RandomPoolDefinition::GetWorldCentricTabPrefix() const
{
	return TEXT("RandomPool ");
}

#undef LOCTEXT_NAMESPACE
