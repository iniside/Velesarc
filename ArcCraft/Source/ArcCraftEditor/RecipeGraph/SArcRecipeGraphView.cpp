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

#include "RecipeGraph/SArcRecipeGraphView.h"

#include "RecipeGraph/ArcRecipeGraph.h"
#include "RecipeGraph/ArcRecipeGraphBuilder.h"
#include "RecipeGraph/ArcRecipeGraphLayout.h"
#include "RecipeGraph/ArcRecipeGraphAddIngredientOptions.h"

#include "GraphEditor.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Engine/AssetManager.h"

#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "RecipeGraph/ArcRecipeGraphNode.h"
#include "EdGraph/EdGraphPin.h"

#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "Core/ArcCoreAssetManager.h"
#include "Editing/ArcJsonRecordEditing.h"
#include "IStructureDetailsView.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "JsonAssets/ArcJsonAssetPaths.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "Misc/MessageDialog.h"
#include "UObject/StructOnScope.h"

#include "Misc/ConfigCacheIni.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SArcRecipeGraphView"

namespace ArcRecipeGraphViewPrivate
{

const TCHAR* RecipeExtension = TEXT("arcrecipe");

static UArcCoreAssetManager* GetAssetManager()
{
	return Cast<UArcCoreAssetManager>(UAssetManager::GetIfInitialized());
}

static const FArcJsonAssetTypeInfo* FindRecordType(const FPrimaryAssetId& Id)
{
	const UArcCoreAssetManager* AssetManager = GetAssetManager();
	return AssetManager != nullptr ? AssetManager->FindJsonRecordType(Id.PrimaryAssetType) : nullptr;
}

static bool IsRecipeRecord(const FPrimaryAssetId& Id)
{
	const FArcJsonAssetTypeInfo* TypeInfo = FindRecordType(Id);
	return TypeInfo != nullptr && TypeInfo->Extension == RecipeExtension;
}

static const FArcRecipeDefinitionData* FindRecipe(const FPrimaryAssetId& Id)
{
	const UArcCoreAssetManager* AssetManager = GetAssetManager();
	return AssetManager != nullptr && IsRecipeRecord(Id) ? AssetManager->FindJsonRecord<FArcRecipeDefinitionData>(Id) : nullptr;
}

static bool IsItemRecord(const FPrimaryAssetId& Id)
{
	const FArcJsonAssetTypeInfo* TypeInfo = FindRecordType(Id);
	return TypeInfo != nullptr && TypeInfo->Extension == FArcItemDefinitionData::JsonExtension();
}

static const FArcItemDefinitionData* FindItem(const FPrimaryAssetId& Id)
{
	const UArcCoreAssetManager* AssetManager = GetAssetManager();
	return AssetManager != nullptr && IsItemRecord(Id) ? AssetManager->FindJsonRecord<FArcItemDefinitionData>(Id) : nullptr;
}

static bool PickRecordFilename(const FArcJsonAssetTypeInfo& Type, const FText& Title, const FString& DefaultName, FString& OutFilename)
{
	FSaveAssetDialogConfig Config;
	Config.DialogTitleOverride = Title;
	Config.DefaultPath = TEXT("/Game");
	Config.DefaultAssetName = DefaultName;
	Config.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;

	FContentBrowserModule& CBM = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	const FString ObjectPath = CBM.Get().CreateModalSaveAssetDialog(Config);
	if (ObjectPath.IsEmpty())
	{
		return false;
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	if (!ArcJsonAssetPaths::ToFilename(ArcJsonAssetPaths::GetContentRoots(), PackageName, OutFilename))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("RecordOutsideRoots", "{0} is not inside the project or a project plugin content folder."), FText::FromString(PackageName)),
			Title);
		return false;
	}
	OutFilename += TEXT(".") + Type.Extension;

	if (!ArcJsonRecordEditing::CreateRecordFile(Type, OutFilename))
	{
		FMessageDialog::Open(EAppMsgType::Ok,
			FText::Format(LOCTEXT("RecordCreateFailed", "{0} could not be created. See the output log."), FText::FromString(OutFilename)),
			Title);
		return false;
	}
	return true;
}

/** Direction-locked BFS: from Start, follow only pins whose Direction == PinDir. */
static void WalkDirection(UArcRecipeGraphNode* Start, EEdGraphPinDirection PinDir, TSet<UArcRecipeGraphNode*>& InOut)
{
	TArray<UArcRecipeGraphNode*> Frontier;
	Frontier.Add(Start);

	while (Frontier.Num() > 0)
	{
		UArcRecipeGraphNode* Current = Frontier.Pop();

		for (UEdGraphPin* Pin : Current->Pins)
		{
			if (Pin == nullptr)
			{
				continue;
			}
			if (Pin->Direction != PinDir)
			{
				continue;
			}
			for (UEdGraphPin* Linked : Pin->LinkedTo)
			{
				if (Linked == nullptr)
				{
					continue;
				}
				UArcRecipeGraphNode* Neighbor = Cast<UArcRecipeGraphNode>(Linked->GetOwningNodeUnchecked());
				if (Neighbor != nullptr && !InOut.Contains(Neighbor))
				{
					InOut.Add(Neighbor);
					Frontier.Add(Neighbor);
				}
			}
		}
	}
}

} // namespace ArcRecipeGraphViewPrivate

void SArcRecipeGraphView::Construct(const FArguments& InArgs)
{
	// Create and root the graph object so the GC does not collect it while
	// SGraphEditor holds a raw pointer to it (mirrors SPhysicsAssetGraph pattern).
	Graph = NewObject<UArcRecipeGraph>(GetTransientPackage());
	Graph->AddToRoot();

	// Bind graph-to-widget delegates so the schema can drive us from context menus.
	Graph->OnIsolateRecipe.BindSP(this, &SArcRecipeGraphView::HandleIsolate);
	Graph->OnShowAll.BindSP(this, &SArcRecipeGraphView::HandleShowAll);
	Graph->OnOpenAsset.BindSP(this, &SArcRecipeGraphView::HandleOpenAsset);
	Graph->OnSelectMember.BindSP(this, &SArcRecipeGraphView::HandleMemberSelected);
	Graph->OnCreateItem.BindSP(this, &SArcRecipeGraphView::HandleCreateItem);
	Graph->OnCreateRecipe.BindSP(this, &SArcRecipeGraphView::HandleCreateRecipe);
	Graph->OnAddDependency.BindSP(this, &SArcRecipeGraphView::HandleAddDependency);

	// Wire selection and double-click events into the graph editor.
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(
		this, &SArcRecipeGraphView::HandleSelectionChanged);
	Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(
		this, &SArcRecipeGraphView::HandleNodeDoubleClicked);

	SAssignNew(GraphEditor, SGraphEditor)
		.GraphToEdit(Graph)
		.IsEditable(true)
		.GraphEvents(Events)
		.ShowGraphStateOverlay(false);

	// Build the details panel shown to the right of the graph.
	FPropertyEditorModule& PropModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs RecordDetailsArgs;
	RecordDetailsArgs.bAllowSearch = true;
	RecordDetailsArgs.bShowOptions = false;
	RecordDetailsArgs.bHideSelectionTip = true;
	RecordDetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	RecordDetailsView = PropModule.CreateStructureDetailView(RecordDetailsArgs, FStructureDetailsViewArgs(), nullptr);
	RecordDetailsView->GetDetailsView()->SetIsPropertyEditingEnabledDelegate(
		FIsPropertyEditingEnabled::CreateLambda([]() -> bool { return false; }));

	if (UArcCoreAssetManager* AssetManager = ArcRecipeGraphViewPrivate::GetAssetManager())
	{
		JsonRecordChangedHandle = AssetManager->OnJsonRecordChanged().AddSP(this, &SArcRecipeGraphView::HandleJsonRecordChanged);
	}

	// Load the persisted layout mode before the combo box resolves its initial
	// selection and before the first RebuildFull() applies a layout.
	{
		int32 PersistedMode = static_cast<int32>(EArcRecipeGraphLayoutMode::MinCrossings);
		GConfig->GetInt(TEXT("ArcCraftEditor"), TEXT("RecipeGraphLayoutMode"), PersistedMode, GEditorPerProjectIni);
		// ForceDirected is the last enumerator — update this clamp when adding new modes.
		const int32 MaxMode = static_cast<int32>(EArcRecipeGraphLayoutMode::ForceDirected);
		if (PersistedMode < 0 || PersistedMode > MaxMode)
		{
			PersistedMode = static_cast<int32>(EArcRecipeGraphLayoutMode::MinCrossings);
		}
		LayoutMode = static_cast<EArcRecipeGraphLayoutMode>(PersistedMode);
	}

	// Populate combo options before SNew(SComboBox) — OptionsSource captures the pointer at construction.
	LayoutModeOptions.Add(MakeShared<EArcRecipeGraphLayoutMode>(EArcRecipeGraphLayoutMode::Compact));
	LayoutModeOptions.Add(MakeShared<EArcRecipeGraphLayoutMode>(EArcRecipeGraphLayoutMode::MinCrossings));
	LayoutModeOptions.Add(MakeShared<EArcRecipeGraphLayoutMode>(EArcRecipeGraphLayoutMode::Straightened));
	LayoutModeOptions.Add(MakeShared<EArcRecipeGraphLayoutMode>(EArcRecipeGraphLayoutMode::Sugiyama));
	LayoutModeOptions.Add(MakeShared<EArcRecipeGraphLayoutMode>(EArcRecipeGraphLayoutMode::ForceDirected));

	// Find the initially selected item matching the loaded mode.
	TSharedPtr<EArcRecipeGraphLayoutMode> InitialModeItem;
	for (const TSharedPtr<EArcRecipeGraphLayoutMode>& Option : LayoutModeOptions)
	{
		if (*Option == LayoutMode)
		{
			InitialModeItem = Option;
			break;
		}
	}

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		+ SSplitter::Slot()
		.Value(0.7f)
		[
			SNew(SVerticalBox)

			// --- Toolbar row -------------------------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f, 4.f, 4.f, 2.f)
			[
				SNew(SHorizontalBox)

				// "Refresh" button: re-enumerate all project assets and rebuild.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("RefreshButton", "Refresh"))
					.ToolTipText(LOCTEXT("RefreshTooltip", "Re-scan all recipe and item assets and rebuild the graph."))
					.OnClicked_Lambda([this]() -> FReply
					{
						RebuildFull();
						return FReply::Handled();
					})
				]

				// "Show All" button: restore full graph after an isolate operation.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ShowAllButton", "Show All"))
					.ToolTipText(LOCTEXT("ShowAllTooltip", "Restore the full dependency graph."))
					.OnClicked_Lambda([this]() -> FReply
					{
						HandleShowAll();
						return FReply::Handled();
					})
				]

				// Layout mode combo box: selects and persists the auto-arrange algorithm.
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SComboBox<TSharedPtr<EArcRecipeGraphLayoutMode>>)
					.OptionsSource(&LayoutModeOptions)
					.InitiallySelectedItem(InitialModeItem)
					.ToolTipText(LOCTEXT("LayoutModeTooltip",
						"Auto-arrange mode applied when the graph is refreshed or when changed here.\n"
						"Layered: Compact — legacy fixed-grid.\n"
						"Layered: Min Crossings — barycenter crossing minimisation + size-aware packing.\n"
						"Layered: Straightened — Min Crossings with chain straightening.\n"
						"Sugiyama (Full) — full Sugiyama with dummy nodes and Brandes-Köpf coordinates.\n"
						"Force-Directed — Fruchterman-Reingold untangle seeded from Min Crossings."))
					.OnSelectionChanged(this, &SArcRecipeGraphView::HandleLayoutModeChanged)
					.OnGenerateWidget_Lambda([](TSharedPtr<EArcRecipeGraphLayoutMode> Item) -> TSharedRef<SWidget>
					{
						return SNew(STextBlock)
							.Text(SArcRecipeGraphView::GetLayoutModeLabel(Item.IsValid()
								? *Item
								: EArcRecipeGraphLayoutMode::MinCrossings));
					})
					[
						SNew(STextBlock)
						.Text_Lambda([this]() -> FText
						{
							return SArcRecipeGraphView::GetLayoutModeLabel(LayoutMode);
						})
					]
				]

				// Search box: jump to and select the first node whose title contains the text.
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(SSearchBox)
					.HintText(LOCTEXT("SearchHint", "Search recipe..."))
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type CommitType)
					{
						if (!GraphEditor.IsValid() || Text.IsEmpty())
						{
							return;
						}

						const FString SearchString = Text.ToString();

						for (UEdGraphNode* Node : Graph->Nodes)
						{
							if (Node == nullptr)
							{
								continue;
							}

							if (Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(SearchString))
							{
								GraphEditor->JumpToNode(Node, /*bRequestRename=*/false);
								return;
							}
						}

						// No match — zoom to fit so the user at least sees the full graph.
						GraphEditor->ZoomToFit(false);
					})
				]
			]

			// --- Graph editor ------------------------------------------------------
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				GraphEditor.ToSharedRef()
			]
		]

		+ SSplitter::Slot()
		.Value(0.3f)
		[
			RecordDetailsView->GetWidget().ToSharedRef()
		]
	];

	// Populate graph with all project assets.  Must be last so the child widget
	// tree is fully constructed before RepopulateFrom touches graph state.
	RebuildFull();
}

SArcRecipeGraphView::~SArcRecipeGraphView()
{
	if (UArcCoreAssetManager* AssetManager = ArcRecipeGraphViewPrivate::GetAssetManager())
	{
		AssetManager->OnJsonRecordChanged().Remove(JsonRecordChangedHandle);
	}
	if (Graph != nullptr)
	{
		Graph->RemoveFromRoot();
	}
}

void SArcRecipeGraphView::RebuildFull()
{
	FullData = FArcRecipeGraphBuilder::BuildFull();
	RepopulateFrom(FullData);
}

void SArcRecipeGraphView::RepopulateFrom(const FArcRecipeGraphData& Data)
{
	FArcRecipeGraphBuilder::PopulateGraph(*Graph, Data);
	InjectPinnedItems();
	FArcRecipeGraphLayout::Apply(*Graph, LayoutMode);
	Graph->NotifyGraphChanged();

	// Defer ZoomToFit: the graph panel may not have been laid out yet at this point,
	// so a direct call is a no-op.  One active-timer tick is enough.
	if (!bPendingZoomToFit)
	{
		bPendingZoomToFit = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(
			this, &SArcRecipeGraphView::HandleDeferredZoomToFit));
	}
}

void SArcRecipeGraphView::HandleIsolate(FPrimaryAssetId RootRecipe)
{
	RepopulateFrom(FArcRecipeGraphBuilder::BuildFocused(FullData, RootRecipe));
}

void SArcRecipeGraphView::HandleShowAll()
{
	RepopulateFrom(FullData);
}

void SArcRecipeGraphView::HandleOpenAsset(FPrimaryAssetId AssetId)
{
	if (ArcRecipeGraphViewPrivate::FindRecordType(AssetId) == nullptr)
	{
		return;
	}
	const UArcCoreAssetManager* AssetManager = ArcRecipeGraphViewPrivate::GetAssetManager();
	const FString Filename = AssetManager != nullptr ? AssetManager->GetJsonRecordFilename(AssetId) : FString();
	if (!Filename.IsEmpty())
	{
		ArcJsonRecordEditing::OpenRecordEditor(Filename);
	}
}

EActiveTimerReturnType SArcRecipeGraphView::HandleDeferredZoomToFit(double /*InCurrentTime*/, float /*InDeltaTime*/)
{
	if (GraphEditor.IsValid())
	{
		GraphEditor->ZoomToFit(false);
	}

	bPendingZoomToFit = false;
	return EActiveTimerReturnType::Stop;
}

void SArcRecipeGraphView::HandleSelectionChanged(const FGraphPanelSelectionSet& NewSelection)
{
	UArcRecipeGraphNode* Selected = nullptr;
	if (NewSelection.Num() == 1)
	{
		for (UObject* Obj : NewSelection)
		{
			Selected = Cast<UArcRecipeGraphNode>(Obj);
		}
	}

	// A node selection supersedes any member-row selection.
	if (Graph != nullptr)
	{
		Graph->SelectedMemberId = FPrimaryAssetId();
	}

	UpdateChainHighlight(Selected);

	if (Selected != nullptr
		&& (Selected->NodeType == EArcRecipeGraphNodeType::Recipe
			|| Selected->NodeType == EArcRecipeGraphNodeType::RawItem))
	{
		ShowRecordInDetails(Selected->AssetId);
	}
	else
	{
		ShowRecordInDetails(FPrimaryAssetId());
	}
}

void SArcRecipeGraphView::HandleNodeDoubleClicked(UEdGraphNode* Node)
{
	UArcRecipeGraphNode* ArcNode = Cast<UArcRecipeGraphNode>(Node);
	if (ArcNode == nullptr)
	{
		return;
	}
	if (ArcNode->NodeType == EArcRecipeGraphNodeType::Recipe
		|| ArcNode->NodeType == EArcRecipeGraphNodeType::RawItem)
	{
		HandleOpenAsset(ArcNode->AssetId);
	}
	// TagGroup: synthetic id, no asset to open.
}

void SArcRecipeGraphView::HandleMemberSelected(FPrimaryAssetId MemberId)
{
	if (Graph != nullptr)
	{
		Graph->SelectedMemberId = MemberId;
	}
	ShowRecordInDetails(MemberId);
}

void SArcRecipeGraphView::ShowRecordInDetails(FPrimaryAssetId RecordId)
{
	RecordSnapshot.Reset();
	if (const FArcRecipeDefinitionData* Recipe = ArcRecipeGraphViewPrivate::FindRecipe(RecordId))
	{
		const UScriptStruct* Struct = FArcRecipeDefinitionData::StaticStruct();
		RecordSnapshot = MakeShared<FStructOnScope>(Struct);
		Struct->CopyScriptStruct(RecordSnapshot->GetStructMemory(), Recipe);
	}
	else if (const FArcItemDefinitionData* Item = ArcRecipeGraphViewPrivate::FindItem(RecordId))
	{
		const UScriptStruct* Struct = FArcItemDefinitionData::StaticStruct();
		RecordSnapshot = MakeShared<FStructOnScope>(Struct);
		Struct->CopyScriptStruct(RecordSnapshot->GetStructMemory(), Item);
	}
	RecordDetailsView->SetStructureData(RecordSnapshot);
}

void SArcRecipeGraphView::HandleJsonRecordChanged(const FPrimaryAssetId& Id)
{
	const FArcJsonAssetTypeInfo* TypeInfo = ArcRecipeGraphViewPrivate::FindRecordType(Id);
	if (bPendingRebuild
		|| (TypeInfo != nullptr
			&& TypeInfo->Extension != ArcRecipeGraphViewPrivate::RecipeExtension
			&& TypeInfo->Extension != FArcItemDefinitionData::JsonExtension()))
	{
		return;
	}
	bPendingRebuild = true;
	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SArcRecipeGraphView::HandleDeferredRebuild));
}

EActiveTimerReturnType SArcRecipeGraphView::HandleDeferredRebuild(double /*InCurrentTime*/, float /*InDeltaTime*/)
{
	bPendingRebuild = false;
	ShowRecordInDetails(FPrimaryAssetId());
	RebuildFull();
	return EActiveTimerReturnType::Stop;
}

void SArcRecipeGraphView::UpdateChainHighlight(UArcRecipeGraphNode* Selected)
{
	if (Graph == nullptr)
	{
		return;
	}

	// No single-node selection -> clear all dimming.
	if (Selected == nullptr)
	{
		Graph->bChainHighlightActive = false;
		for (UEdGraphNode* EdNode : Graph->Nodes)
		{
			if (UArcRecipeGraphNode* ArcNode = Cast<UArcRecipeGraphNode>(EdNode))
			{
				ArcNode->HighlightState = EArcRecipeNodeHighlight::Normal;
			}
		}
		return;
	}

	// Two direction-locked BFS walks: upstream producers (Input pins) and
	// downstream consumers (Output pins).  Only nodes reachable along the
	// dependency spine are kept Normal; all others are Dimmed.
	TSet<UArcRecipeGraphNode*> InChain;
	InChain.Add(Selected);
	ArcRecipeGraphViewPrivate::WalkDirection(Selected, EGPD_Input, InChain);
	ArcRecipeGraphViewPrivate::WalkDirection(Selected, EGPD_Output, InChain);

	for (UEdGraphNode* EdNode : Graph->Nodes)
	{
		if (UArcRecipeGraphNode* ArcNode = Cast<UArcRecipeGraphNode>(EdNode))
		{
			ArcNode->HighlightState = InChain.Contains(ArcNode)
				? EArcRecipeNodeHighlight::Normal
				: EArcRecipeNodeHighlight::Dimmed;
		}
	}
	Graph->bChainHighlightActive = true;
}

// ---------------------------------------------------------------------------
// HandleCreateItem
// ---------------------------------------------------------------------------

void SArcRecipeGraphView::HandleCreateItem()
{
	const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByExtension(FArcItemDefinitionData::JsonExtension());
	FString Filename;
	if (Type == nullptr || !ArcRecipeGraphViewPrivate::PickRecordFilename(*Type, LOCTEXT("CreateItemTitle", "Create Item"), TEXT("NewItem"), Filename))
	{
		return;
	}

	ArcJsonRecordEditing::OpenRecordEditor(Filename);
	if (const UArcJsonRecordEditObject* EditRecord = ArcJsonRecordEditing::FindRecord(Filename))
	{
		PinnedItemIds.AddUnique(ArcJsonAssets::MakeRecordId(*Type, EditRecord->GetRecord()));
	}
	RebuildFull();
}

// ---------------------------------------------------------------------------
// HandleCreateRecipe
// ---------------------------------------------------------------------------

void SArcRecipeGraphView::HandleCreateRecipe()
{
	const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByExtension(ArcRecipeGraphViewPrivate::RecipeExtension);
	FString Filename;
	if (Type == nullptr || !ArcRecipeGraphViewPrivate::PickRecordFilename(*Type, LOCTEXT("CreateRecipeTitle", "Create Recipe"), TEXT("NewRecipe"), Filename))
	{
		return;
	}

	ArcJsonRecordEditing::OpenRecordEditor(Filename);
	RebuildFull();
}

// ---------------------------------------------------------------------------
// HandleAddDependency
// ---------------------------------------------------------------------------

void SArcRecipeGraphView::HandleAddDependency(UArcRecipeGraphNode* Source, UArcRecipeGraphNode* TargetRecipe)
{
	if (Source == nullptr || TargetRecipe == nullptr)
	{
		return;
	}

	PendingDependency.bValid           = true;
	PendingDependency.SourceType       = Source->NodeType;
	PendingDependency.SourceId         = Source->AssetId;
	PendingDependency.SourceGroupTags  = Source->GroupTags;
	PendingDependency.TargetRecipeId   = TargetRecipe->AssetId;

	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(
		this, &SArcRecipeGraphView::ApplyPendingDependency));
}

// ---------------------------------------------------------------------------
// ApplyPendingDependency
// ---------------------------------------------------------------------------

EActiveTimerReturnType SArcRecipeGraphView::ApplyPendingDependency(double /*InCurrentTime*/, float /*InDeltaTime*/)
{
	if (!PendingDependency.bValid)
	{
		return EActiveTimerReturnType::Stop;
	}

	FPendingDependency P = PendingDependency;
	PendingDependency.bValid = false;

	const UArcCoreAssetManager* AssetManager = ArcRecipeGraphViewPrivate::GetAssetManager();
	const FString TargetFilename = AssetManager != nullptr && ArcRecipeGraphViewPrivate::FindRecipe(P.TargetRecipeId) != nullptr
		? AssetManager->GetJsonRecordFilename(P.TargetRecipeId)
		: FString();
	if (TargetFilename.IsEmpty())
	{
		return EActiveTimerReturnType::Stop;
	}

	// Resolve the source item id (used when the ingredient kind is ItemDef).
	FPrimaryAssetId SourceItemId;
	if (P.SourceType == EArcRecipeGraphNodeType::RawItem)
	{
		SourceItemId = P.SourceId;
	}
	else if (P.SourceType == EArcRecipeGraphNodeType::Recipe)
	{
		const FArcRecipeDefinitionData* SrcRecipe = ArcRecipeGraphViewPrivate::FindRecipe(P.SourceId);
		const FArcRecipeOutput* SrcMainOutput = SrcRecipe != nullptr ? SrcRecipe->GetMainOutput() : nullptr;
		if (SrcMainOutput != nullptr)
		{
			SourceItemId = SrcMainOutput->ItemDefinition.AssetId;
		}
	}
	// TagGroup -> SourceItemId stays invalid; will use Tag ingredient kind.

	TStrongObjectPtr<UArcRecipeGraphAddIngredientOptions> Options(
		NewObject<UArcRecipeGraphAddIngredientOptions>(GetTransientPackage()));

	if (P.SourceType == EArcRecipeGraphNodeType::TagGroup)
	{
		Options->Kind         = EArcAddIngredientKind::Tag;
		Options->RequiredTags = P.SourceGroupTags;
	}
	else
	{
		Options->Kind = EArcAddIngredientKind::ItemDef;
	}

	if (!PromptAddIngredient(Options.Get()))
	{
		return EActiveTimerReturnType::Stop;
	}

	// Build the ingredient FInstancedStruct.
	FInstancedStruct IS;
	if (Options->Kind == EArcAddIngredientKind::ItemDef)
	{
		if (!SourceItemId.IsValid())
		{
			return EActiveTimerReturnType::Stop;
		}
		FArcRecipeIngredient_ItemDef I;
		I.Amount          = Options->Amount;
		I.bConsumeOnCraft = Options->bConsumeOnCraft;
		I.MinRarityStep   = Options->MinRarityStep;
		I.bOptional       = Options->bOptional;
		I.ItemDefinitionId.AssetId = SourceItemId;
		IS.InitializeAs<FArcRecipeIngredient_ItemDef>(I);
	}
	else
	{
		FArcRecipeIngredient_Tags T;
		T.Amount          = Options->Amount;
		T.bConsumeOnCraft = Options->bConsumeOnCraft;
		T.RequiredTags    = Options->RequiredTags;
		T.DenyTags        = Options->DenyTags;
		T.MinRarityStep   = Options->MinRarityStep;
		T.bOptional       = Options->bOptional;
		IS.InitializeAs<FArcRecipeIngredient_Tags>(T);
	}

	UArcJsonRecordEditObject* EditRecord = ArcJsonRecordEditing::AcquireRecord(TargetFilename);
	if (EditRecord == nullptr)
	{
		return EActiveTimerReturnType::Stop;
	}
	if (FArcRecipeDefinitionData* Recipe = EditRecord->GetMutableRecord().GetMutablePtr<FArcRecipeDefinitionData>())
	{
		const FScopedTransaction Tx(LOCTEXT("AddIngredient", "Add Recipe Ingredient"));
		EditRecord->Modify();
		Recipe->Ingredients.Add(MoveTemp(IS));
		EditRecord->PostEditChange();
	}
	ArcJsonRecordEditing::OpenRecordEditor(TargetFilename);
	ArcJsonRecordEditing::ReleaseRecord(EditRecord);
	return EActiveTimerReturnType::Stop;
}

// ---------------------------------------------------------------------------
// PromptAddIngredient
// ---------------------------------------------------------------------------

bool SArcRecipeGraphView::PromptAddIngredient(UArcRecipeGraphAddIngredientOptions* Options)
{
	FPropertyEditorModule& PEM = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs Args;
	Args.bAllowSearch       = false;
	Args.NameAreaSettings   = FDetailsViewArgs::HideNameArea;
	TSharedRef<IDetailsView> DV = PEM.CreateDetailView(Args);
	DV->SetObject(Options);

	TSharedRef<bool> bAccepted = MakeShared<bool>(false);

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("AddIngredientTitle", "Add Ingredient"))
		.ClientSize(FVector2D(400.f, 300.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				DV
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 8.f, 0.f)
				[
					SNew(SButton)
					.Text(LOCTEXT("OKButton", "OK"))
					.OnClicked_Lambda([bAccepted, &Window]() -> FReply
					{
						*bAccepted = true;
						Window->RequestDestroyWindow();
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("CancelButton", "Cancel"))
					.OnClicked_Lambda([&Window]() -> FReply
					{
						Window->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
			]
		];

	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	FSlateApplication::Get().AddModalWindow(Window, RootWindow);

	return *bAccepted;
}

// ---------------------------------------------------------------------------
// InjectPinnedItems
// ---------------------------------------------------------------------------

void SArcRecipeGraphView::InjectPinnedItems()
{
	if (Graph == nullptr || PinnedItemIds.Num() == 0)
	{
		return;
	}

	for (const FPrimaryAssetId& Id : PinnedItemIds)
	{
		// Skip if already present (builder emitted this node).
		bool bAlreadyPresent = false;
		for (UEdGraphNode* EdNode : Graph->Nodes)
		{
			UArcRecipeGraphNode* ArcNode = Cast<UArcRecipeGraphNode>(EdNode);
			if (ArcNode != nullptr && ArcNode->AssetId == Id)
			{
				bAlreadyPresent = true;
				break;
			}
		}
		if (bAlreadyPresent)
		{
			continue;
		}

		FText DisplayTitle = FText::FromName(Id.PrimaryAssetName);
		const FArcItemDefinitionData* ItemDef = ArcRecipeGraphViewPrivate::FindItem(Id);
		if (ItemDef != nullptr)
		{
			const FArcItemFragment_UIDescription* UIDesc =
				ItemDef->FindFragment<FArcItemFragment_UIDescription>();
			if (UIDesc != nullptr && !UIDesc->ItemName.IsEmpty())
			{
				DisplayTitle = UIDesc->ItemName;
			}
		}

		UArcRecipeGraphNode* Node = NewObject<UArcRecipeGraphNode>(Graph);
		Node->NodeType     = EArcRecipeGraphNodeType::RawItem;
		Node->AssetId      = Id;
		Node->DisplayTitle = DisplayTitle;
		Node->AllocateDefaultPins();
		Graph->AddNode(Node, /*bUserAction=*/false, /*bSelectNewNode=*/false);
	}
}

// ---------------------------------------------------------------------------
// HandleLayoutModeChanged
// ---------------------------------------------------------------------------

void SArcRecipeGraphView::HandleLayoutModeChanged(
	TSharedPtr<EArcRecipeGraphLayoutMode> NewMode,
	ESelectInfo::Type /*SelectInfo*/)
{
	if (!NewMode.IsValid())
	{
		return;
	}

	LayoutMode = *NewMode;

	// Persist the selection so it survives editor restarts.
	GConfig->SetInt(
		TEXT("ArcCraftEditor"),
		TEXT("RecipeGraphLayoutMode"),
		static_cast<int32>(LayoutMode),
		GEditorPerProjectIni);

	// Apply must read measured widget sizes (DEPRECATED_NodeWidget) before
	// NotifyGraphChanged rebuilds node widgets — ordering is mandatory.
	FArcRecipeGraphLayout::Apply(*Graph, LayoutMode);
	Graph->NotifyGraphChanged();

	// Deferred zoom so the panel geometry is settled after the re-layout.
	// bPendingZoomToFit guards against double-registration if a repopulate
	// already queued a zoom this frame.
	if (!bPendingZoomToFit)
	{
		bPendingZoomToFit = true;
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(
			this, &SArcRecipeGraphView::HandleDeferredZoomToFit));
	}
}

// ---------------------------------------------------------------------------
// GetLayoutModeLabel
// ---------------------------------------------------------------------------

FText SArcRecipeGraphView::GetLayoutModeLabel(EArcRecipeGraphLayoutMode Mode)
{
	switch (Mode)
	{
		case EArcRecipeGraphLayoutMode::Compact:
			return LOCTEXT("LayoutCompact",      "Layered: Compact");
		case EArcRecipeGraphLayoutMode::MinCrossings:
			return LOCTEXT("LayoutMinCrossings", "Layered: Min Crossings");
		case EArcRecipeGraphLayoutMode::Straightened:
			return LOCTEXT("LayoutStraightened", "Layered: Straightened");
		case EArcRecipeGraphLayoutMode::Sugiyama:
			return LOCTEXT("LayoutSugiyama",     "Sugiyama (Full)");
		case EArcRecipeGraphLayoutMode::ForceDirected:
			return LOCTEXT("LayoutForce",        "Force-Directed");
		default:
			return LOCTEXT("LayoutUnknown",      "Unknown");
	}
}

#undef LOCTEXT_NAMESPACE
