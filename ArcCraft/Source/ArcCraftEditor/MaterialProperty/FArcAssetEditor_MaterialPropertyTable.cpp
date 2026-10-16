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

#include "MaterialProperty/FArcAssetEditor_MaterialPropertyTable.h"

#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableData.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "MaterialProperty/ArcMaterialPropertyTable_ColumnSchema.h"
#include "MaterialProperty/ArcMaterialPropertyTable_DebugEval.h"
#include "MaterialProperty/ArcMaterialPropertyTableDetailCustomization.h"
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
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "FArcAssetEditor_MaterialPropertyTable"

const FName FArcAssetEditor_MaterialPropertyTable::DetailsTabId(TEXT("MaterialPropertyTableEditor_Details"));
const FName FArcAssetEditor_MaterialPropertyTable::RuleTableTabId(TEXT("MaterialPropertyTableEditor_Table"));
const FName FArcAssetEditor_MaterialPropertyTable::DebugTabId(TEXT("MaterialPropertyTableEditor_Debug"));
const FName FArcAssetEditor_MaterialPropertyTable::ToolkitFName(TEXT("ArcMaterialPropertyTableEditor"));
const FName FArcAssetEditor_MaterialPropertyTable::AppIdentifier(TEXT("ArcMaterialPropertyTableEditorApp"));

// ---------------------------------------------------------------------------
// Row builder for SArcModifierTreeView
// ---------------------------------------------------------------------------

static TArray<TSharedPtr<FArcModifierTreeItem>> BuildMaterialPropertyTableRows(
	UArcJsonRecordEditObject* Record,
	const FArcModifierDebugContext& /*DebugCtx*/)
{
	TArray<TSharedPtr<FArcModifierTreeItem>> Rows;

	const FArcMaterialPropertyTableData* Table = ArcCraftRecordToolkit::GetData<FArcMaterialPropertyTableData>(Record);
	if (Table == nullptr)
	{
		return Rows;
	}

	for (int32 RuleIdx = 0; RuleIdx < Table->Rules.Num(); ++RuleIdx)
	{
		TSharedPtr<FArcModifierTreeItem> RuleItem = MakeShared<FArcModifierTreeItem>();
		RuleItem->Kind = EArcModifierRowKind::Rule;
		RuleItem->ParentIndex = INDEX_NONE;
		RuleItem->ChildIndex = RuleIdx;

		// Build child modifier rows
		const FArcMaterialPropertyRule& Rule = Table->Rules[RuleIdx];
		for (int32 ModIdx = 0; ModIdx < Rule.Modifiers.Num(); ++ModIdx)
		{
			TSharedPtr<FArcModifierTreeItem> ModItem = MakeShared<FArcModifierTreeItem>();
			ModItem->Kind = EArcModifierRowKind::Modifier;
			ModItem->ParentIndex = RuleIdx;
			ModItem->ChildIndex = ModIdx;
			RuleItem->Children.Add(ModItem);
		}

		Rows.Add(RuleItem);
	}

	return Rows;
}

// ---------------------------------------------------------------------------
// Open / lifetime
// ---------------------------------------------------------------------------

bool FArcAssetEditor_MaterialPropertyTable::OpenRecord(const FString& Filename)
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
	if (ArcCraftRecordToolkit::GetData<FArcMaterialPropertyTableData>(EditRecord) == nullptr)
	{
		ArcJsonRecordEditing::ReleaseRecord(EditRecord);
		return false;
	}
	TSharedRef<FArcAssetEditor_MaterialPropertyTable> NewEditor(new FArcAssetEditor_MaterialPropertyTable());
	NewEditor->InitEditor(EditRecord);
	return true;
}

FArcAssetEditor_MaterialPropertyTable::~FArcAssetEditor_MaterialPropertyTable()
{
	if (DebugToolbar.IsValid())
	{
		UArcCraftEditorDebugSettings::SaveDebugContext(RecordId, DebugToolbar->GetContext());
	}
	ReleaseRecord();
}

void FArcAssetEditor_MaterialPropertyTable::ReleaseRecord()
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

void FArcAssetEditor_MaterialPropertyTable::InitEditor(UArcJsonRecordEditObject* InRecord)
{
	Record = InRecord;
	RecordId = ArcCraftRecordToolkit::GetRecordId(*InRecord);
	RecordChangedHandle = InRecord->OnChanged().AddSP(this, &FArcAssetEditor_MaterialPropertyTable::HandleRecordChanged);

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
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcMaterialPropertyTableDetailCustomization::MakeInstance));
	RefreshDetails();

	// Per-row details view — persistent IDetailsView backed by transient UObject proxies.
	RowDetailsArgs.bAllowSearch = true;
	RowDetailsArgs.bShowOptions = false;
	RowDetailsArgs.bShowObjectLabel = false;
	RowDetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	RowDetailsView = PropModule.CreateDetailView(RowDetailsArgs);
	RowDetailsView->RegisterInstancedCustomPropertyLayout(
		UArcCraftRuleRowProxy::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcCraftRowProxyDetailsCustomization::MakeInstance));
	RowDetailsView->RegisterInstancedCustomPropertyLayout(
		UArcCraftModifierRowProxy::StaticClass(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcCraftRowProxyDetailsCustomization::MakeInstance));
	RowDetailsView->OnFinishedChangingProperties().AddSP(
		this, &FArcAssetEditor_MaterialPropertyTable::HandleRowPropertyChanged);

	// Construct proxy objects — kept alive for the lifetime of this toolkit.
	RuleProxy = TStrongObjectPtr<UArcCraftRuleRowProxy>(
		NewObject<UArcCraftRuleRowProxy>(GetTransientPackage()));
	ModifierProxy = TStrongObjectPtr<UArcCraftModifierRowProxy>(
		NewObject<UArcCraftModifierRowProxy>(GetTransientPackage()));

	// Host widget — assigned once; content managed by ShowRowSelection.
	SAssignNew(RowDetailsHost, SBox)
	[
		RowDetailsView.ToSharedRef()
	];

	// Rule table widget
	TArray<FArcModifierColumn> RuleSchema = ArcMaterialPropertyTable_ColumnSchema::GetRuleRowSchema();
	TArray<FArcModifierColumn> ModifierSchema = ArcMaterialPropertyTable_ColumnSchema::GetModifierChildRowSchema();

	// Merge both schemas into one array for SArcModifierTreeView
	TArray<FArcModifierColumn> CombinedSchema;
	CombinedSchema.Append(MoveTemp(RuleSchema));
	CombinedSchema.Append(MoveTemp(ModifierSchema));

	SAssignNew(RuleTableWidget, SArcModifierTreeView)
		.Record(Record)
		.ColumnSchema(CombinedSchema)
		.AddTopLevelLabel(LOCTEXT("AddRule", "+ Add Rule"))
		.TopLevelKind(EArcModifierRowKind::Rule)
		.OnBuildRows(FArcModifierBuildRowsFn::CreateStatic(&BuildMaterialPropertyTableRows))
		.OnEvaluateDebug(FArcModifierDebugEvaluatorFn::CreateLambda(
			[](UArcJsonRecordEditObject* EditRecord, const FArcModifierDebugContext& Ctx) -> FArcModifierDebugEvaluation
			{
				const FArcMaterialPropertyTableData* Table = ArcCraftRecordToolkit::GetData<FArcMaterialPropertyTableData>(EditRecord);
				if (Table == nullptr) { return FArcModifierDebugEvaluation(); }
				return ArcMaterialPropertyTable_DebugEval::EvaluateMaterialPropertyTableForDebug(*Table, Ctx);
			}))
		.OnAddTopLevel(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::HandleAddRule))
		.OnAddChildOfSelected(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::HandleAddModifierUnderRule))
		.OnDuplicateRow(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::HandleDuplicateRow))
		.OnDeleteRow(FArcOnModifierRowAction::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::HandleDeleteRow))
		.OnReorderTopLevel(FArcOnModifierRowMove::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::HandleReorderTopLevel))
		.OnRowSelected(FArcOnModifierRowSelected::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::HandleRowSelected))
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
				this, &FArcAssetEditor_MaterialPropertyTable::OnDebugContextChanged));

	DebugToolbar->SetDebugContext(UArcCraftEditorDebugSettings::LoadDebugContext(RecordId));

	// Tab layout
	const TSharedRef<FTabManager::FLayout> Layout =
		FTabManager::NewLayout("ArcMaterialPropertyTableEditor_v1")
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
					->AddTab(RuleTableTabId, ETabState::OpenedTab)
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

void FArcAssetEditor_MaterialPropertyTable::RegisterTabSpawners(
	const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(
		LOCTEXT("WorkspaceMenu", "Material Property Table Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(RuleTableTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::SpawnTab_RuleTable))
		.SetDisplayName(LOCTEXT("RuleTableTab", "Rules"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DebugTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::SpawnTab_Debug))
		.SetDisplayName(LOCTEXT("DebugTab", "Debug"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(DetailsTabId,
		FOnSpawnTab::CreateSP(this, &FArcAssetEditor_MaterialPropertyTable::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
}

void FArcAssetEditor_MaterialPropertyTable::UnregisterTabSpawners(
	const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(RuleTableTabId);
	InTabManager->UnregisterTabSpawner(DebugTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
}

TSharedRef<SDockTab> FArcAssetEditor_MaterialPropertyTable::SpawnTab_Details(
	const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTitle", "Details"))
		[
			DetailsView->GetWidget().ToSharedRef()
		];
}

TSharedRef<SDockTab> FArcAssetEditor_MaterialPropertyTable::SpawnTab_RuleTable(
	const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("RuleTableTitle", "Rules"))
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			+ SSplitter::Slot()
			.Value(0.6f)
			[
				RuleTableWidget.ToSharedRef()
			]

			+ SSplitter::Slot()
			.Value(0.4f)
			[
				RowDetailsHost.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FArcAssetEditor_MaterialPropertyTable::SpawnTab_Debug(
	const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DebugTitle", "Debug"))
		[
			SNew(SVerticalBox)

			// Standard toolbar (R slider, ingredient tags, slot tags, live preview)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				DebugToolbar.ToSharedRef()
			]

			// Informational note — no Roll button for MPT (all eligible rules fire)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(6.0f, 4.0f))
				[
					SNew(STextBlock)
					.Text(LOCTEXT("NoRollNote",
						"All matching rules fire simultaneously — there is no random selection "
						"for Material Property Tables. Rows highlighted green match the current "
						"debug context; red rows show the failure reason on hover."))
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
					.AutoWrapText(true)
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

FArcMaterialPropertyTableData* FArcAssetEditor_MaterialPropertyTable::GetData() const
{
	return ArcCraftRecordToolkit::GetData<FArcMaterialPropertyTableData>(Record.Get());
}

void FArcAssetEditor_MaterialPropertyTable::HandleRecordChanged(UArcJsonRecordEditObject* Object, EArcJsonRecordEditChange Change)
{
	switch (Change)
	{
	case EArcJsonRecordEditChange::Edited:
		if (!bApplyingEdit)
		{
			RefreshActiveRow();
			RuleTableWidget->RefreshRows();
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

void FArcAssetEditor_MaterialPropertyTable::RefreshFromRecord()
{
	RefreshDetails();
	RuleTableWidget->RebuildAll();
	RuleTableWidget->SetDebugContext(DebugToolbar->GetContext());
}

void FArcAssetEditor_MaterialPropertyTable::RefreshDetails()
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

void FArcAssetEditor_MaterialPropertyTable::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		EditRecord->Modify();
	}
}

void FArcAssetEditor_MaterialPropertyTable::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (UArcJsonRecordEditObject* EditRecord = Record.Get())
	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		EditRecord->PostEditChange();
	}
}

void FArcAssetEditor_MaterialPropertyTable::OnDebugContextChanged(
	const FArcModifierDebugContext& NewContext)
{
	if (RuleTableWidget.IsValid())
	{
		RuleTableWidget->SetDebugContext(NewContext);
	}

	UArcCraftEditorDebugSettings::SaveDebugContext(RecordId, NewContext);
}

// ---------------------------------------------------------------------------
// ShowRowSelection
// ---------------------------------------------------------------------------

void FArcAssetEditor_MaterialPropertyTable::ShowRowSelection(TSharedPtr<FArcModifierTreeItem> Selected)
{
	const FArcMaterialPropertyTableData* Table = GetData();
	if (Selected.IsValid() && Table != nullptr)
	{
		if (Selected->Kind == EArcModifierRowKind::Rule
			&& Table->Rules.IsValidIndex(Selected->ChildIndex))
		{
			RuleProxy->Data = Table->Rules[Selected->ChildIndex];
			ActiveKind = ERowKind::Rule;
			ActiveParentIdx = INDEX_NONE;
			ActiveChildIdx = Selected->ChildIndex;
			RowDetailsView->SetObject(RuleProxy.Get(), true);
			return;
		}
		if (Selected->Kind == EArcModifierRowKind::Modifier
			&& Table->Rules.IsValidIndex(Selected->ParentIndex)
			&& Table->Rules[Selected->ParentIndex].Modifiers.IsValidIndex(Selected->ChildIndex))
		{
			ModifierProxy->Data = Table->Rules[Selected->ParentIndex].Modifiers[Selected->ChildIndex];
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

void FArcAssetEditor_MaterialPropertyTable::RefreshActiveRow()
{
	const FArcMaterialPropertyTableData* Table = GetData();
	if (Table == nullptr)
	{
		return;
	}
	if (ActiveKind == ERowKind::Rule && Table->Rules.IsValidIndex(ActiveChildIdx))
	{
		RuleProxy->Data = Table->Rules[ActiveChildIdx];
		RowDetailsView->ForceRefresh();
	}
	else if (ActiveKind == ERowKind::Modifier
		&& Table->Rules.IsValidIndex(ActiveParentIdx)
		&& Table->Rules[ActiveParentIdx].Modifiers.IsValidIndex(ActiveChildIdx))
	{
		ModifierProxy->Data = Table->Rules[ActiveParentIdx].Modifiers[ActiveChildIdx];
		RowDetailsView->ForceRefresh();
	}
}

// ---------------------------------------------------------------------------
// Action handlers
// ---------------------------------------------------------------------------

void FArcAssetEditor_MaterialPropertyTable::HandleAddRule(TSharedPtr<FArcModifierTreeItem> /*Selected*/)
{
	if (GetData() == nullptr)
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("AddRule", "Add Rule"), [this]()
		{
			TArray<FArcMaterialPropertyRule>& Rules = GetData()->Rules;
			FArcMaterialPropertyRule& NewRule = Rules.AddDefaulted_GetRef();
			NewRule.RuleName = FText::FromString(FString::Printf(TEXT("Rule %d"), Rules.Num()));
		});
	}

	const int32 NewRuleIdx = GetData()->Rules.Num() - 1;
	RuleTableWidget->RebuildAll();
	RuleTableWidget->SelectByKey(EArcModifierRowKind::Rule, INDEX_NONE, NewRuleIdx);
}

void FArcAssetEditor_MaterialPropertyTable::HandleAddModifierUnderRule(TSharedPtr<FArcModifierTreeItem> Selected)
{
	if (!Selected.IsValid() || Selected->Kind != EArcModifierRowKind::Rule)
	{
		return;
	}

	const FArcMaterialPropertyTableData* Table = GetData();
	const int32 RuleIdx = Selected->ChildIndex;
	if (Table == nullptr || !Table->Rules.IsValidIndex(RuleIdx))
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("AddModifier", "Add Modifier"), [this, RuleIdx]()
		{
			GetData()->Rules[RuleIdx].Modifiers.Emplace(FInstancedStruct::Make<FArcCraftModifier_Stats>());
		});
	}

	const int32 NewModIdx = GetData()->Rules[RuleIdx].Modifiers.Num() - 1;
	RuleTableWidget->RebuildAll();
	RuleTableWidget->SelectByKey(EArcModifierRowKind::Modifier, RuleIdx, NewModIdx);
}

void FArcAssetEditor_MaterialPropertyTable::HandleDuplicateRow(TSharedPtr<FArcModifierTreeItem> Selected)
{
	const FArcMaterialPropertyTableData* Table = GetData();
	if (!Selected.IsValid() || Table == nullptr)
	{
		return;
	}

	const EArcModifierRowKind Kind = Selected->Kind;
	const int32 ParentIndex = Selected->ParentIndex;
	const int32 ChildIndex = Selected->ChildIndex;
	const bool bValidRule = Kind == EArcModifierRowKind::Rule && Table->Rules.IsValidIndex(ChildIndex);
	const bool bValidModifier = Kind == EArcModifierRowKind::Modifier
		&& Table->Rules.IsValidIndex(ParentIndex)
		&& Table->Rules[ParentIndex].Modifiers.IsValidIndex(ChildIndex);
	if (!bValidRule && !bValidModifier)
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("DuplicateRow", "Duplicate Row"), [this, bValidRule, ParentIndex, ChildIndex]()
		{
			TArray<FArcMaterialPropertyRule>& Rules = GetData()->Rules;
			if (bValidRule)
			{
				FArcMaterialPropertyRule Copy = Rules[ChildIndex];
				Rules.Insert(MoveTemp(Copy), ChildIndex + 1);
			}
			else
			{
				TArray<FInstancedStruct>& Modifiers = Rules[ParentIndex].Modifiers;
				FInstancedStruct Copy = Modifiers[ChildIndex];
				Modifiers.Insert(MoveTemp(Copy), ChildIndex + 1);
			}
		});
	}

	RuleTableWidget->RebuildAll();
}

void FArcAssetEditor_MaterialPropertyTable::HandleDeleteRow(TSharedPtr<FArcModifierTreeItem> Selected)
{
	const FArcMaterialPropertyTableData* Table = GetData();
	if (!Selected.IsValid() || Table == nullptr)
	{
		return;
	}

	const EArcModifierRowKind Kind = Selected->Kind;
	const int32 ParentIndex = Selected->ParentIndex;
	const int32 ChildIndex = Selected->ChildIndex;
	const bool bValidRule = Kind == EArcModifierRowKind::Rule && Table->Rules.IsValidIndex(ChildIndex);
	const bool bValidModifier = Kind == EArcModifierRowKind::Modifier
		&& Table->Rules.IsValidIndex(ParentIndex)
		&& Table->Rules[ParentIndex].Modifiers.IsValidIndex(ChildIndex);
	if (!bValidRule && !bValidModifier)
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("DeleteRow", "Delete Row"), [this, bValidRule, ParentIndex, ChildIndex]()
		{
			TArray<FArcMaterialPropertyRule>& Rules = GetData()->Rules;
			if (bValidRule)
			{
				Rules.RemoveAt(ChildIndex);
			}
			else
			{
				Rules[ParentIndex].Modifiers.RemoveAt(ChildIndex);
			}
		});
	}

	RuleTableWidget->RebuildAll();
}

void FArcAssetEditor_MaterialPropertyTable::HandleReorderTopLevel(int32 From, int32 To)
{
	const FArcMaterialPropertyTableData* Table = GetData();
	if (Table == nullptr)
	{
		return;
	}

	const int32 RuleCount = Table->Rules.Num();
	if (RuleCount == 0 || From == To)
	{
		return;
	}

	To = FMath::Clamp(To, 0, RuleCount - 1);
	if (From == To || !Table->Rules.IsValidIndex(From))
	{
		return;
	}

	{
		TGuardValue<bool> Guard(bApplyingEdit, true);
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("ReorderRule", "Reorder Rule"), [this, From, To]()
		{
			TArray<FArcMaterialPropertyRule>& Rules = GetData()->Rules;
			FArcMaterialPropertyRule Tmp = MoveTemp(Rules[From]);
			Rules.RemoveAt(From);
			Rules.Insert(MoveTemp(Tmp), To);
		});
	}

	RuleTableWidget->RebuildAll();
}

void FArcAssetEditor_MaterialPropertyTable::HandleRowSelected(TSharedPtr<FArcModifierTreeItem> Selected)
{
	ShowRowSelection(Selected);
}

void FArcAssetEditor_MaterialPropertyTable::HandleRowPropertyChanged(const FPropertyChangedEvent& Event)
{
	if ((Event.ChangeType & EPropertyChangeType::Interactive) != 0)
	{
		return;
	}

	const FArcMaterialPropertyTableData* Table = GetData();
	if (Table == nullptr)
	{
		return;
	}

	const int32 ParentIndex = ActiveParentIdx;
	const int32 ChildIndex = ActiveChildIdx;
	TGuardValue<bool> Guard(bApplyingEdit, true);
	if (ActiveKind == ERowKind::Rule && Table->Rules.IsValidIndex(ChildIndex))
	{
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("EditRuleRow", "Edit Rule"), [this, ChildIndex]()
		{
			GetData()->Rules[ChildIndex] = RuleProxy->Data;
		});
	}
	else if (ActiveKind == ERowKind::Modifier
		&& Table->Rules.IsValidIndex(ParentIndex)
		&& Table->Rules[ParentIndex].Modifiers.IsValidIndex(ChildIndex))
	{
		ArcCraftRecordToolkit::Edit(Record.Get(), LOCTEXT("EditModifierRow", "Edit Modifier"), [this, ParentIndex, ChildIndex]()
		{
			GetData()->Rules[ParentIndex].Modifiers[ChildIndex] = ModifierProxy->Data;
		});
	}
}

// ---------------------------------------------------------------------------
// Save / close
// ---------------------------------------------------------------------------

bool FArcAssetEditor_MaterialPropertyTable::CanSaveAsset() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr && EditRecord->IsOpen();
}

void FArcAssetEditor_MaterialPropertyTable::SaveAsset_Execute()
{
	ArcCraftRecordToolkit::Save(Record.Get());
}

void FArcAssetEditor_MaterialPropertyTable::FindInContentBrowser_Execute()
{
	if (const UArcJsonRecordEditObject* Object = Record.Get())
	{
		ArcJsonRecordEditing::BrowseToRecordFile(Object->GetFilename());
	}
}

bool FArcAssetEditor_MaterialPropertyTable::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	return ArcCraftRecordToolkit::ConfirmClose(Record.Get(), DiscardOnCloseFrame);
}

void FArcAssetEditor_MaterialPropertyTable::OnClose()
{
	bClosing = true;
	ArcCraftRecordToolkit::CommitClose(Record.Get(), DiscardOnCloseFrame);
	ReleaseRecord();
}

// ---------------------------------------------------------------------------
// Toolkit metadata
// ---------------------------------------------------------------------------

FText FArcAssetEditor_MaterialPropertyTable::GetBaseToolkitName() const
{
	return LOCTEXT("BaseToolkitName", "Material Property Table Editor");
}

FText FArcAssetEditor_MaterialPropertyTable::GetToolkitName() const
{
	const FText Name = ArcCraftRecordToolkit::GetRecordName(Record.Get());
	return Name.IsEmpty() ? GetBaseToolkitName() : Name;
}

FText FArcAssetEditor_MaterialPropertyTable::GetTabSuffix() const
{
	return ArcCraftRecordToolkit::GetDirtySuffix(Record.Get());
}

FText FArcAssetEditor_MaterialPropertyTable::GetToolkitToolTipText() const
{
	const UArcJsonRecordEditObject* EditRecord = Record.Get();
	return EditRecord != nullptr ? FText::FromString(EditRecord->GetFilename()) : GetBaseToolkitName();
}

FLinearColor FArcAssetEditor_MaterialPropertyTable::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.28f, 0.57f, 0.75f, 0.5f); // Steel blue to match asset color
}

FString FArcAssetEditor_MaterialPropertyTable::GetWorldCentricTabPrefix() const
{
	return TEXT("MaterialPropertyTable ");
}

#undef LOCTEXT_NAMESPACE
