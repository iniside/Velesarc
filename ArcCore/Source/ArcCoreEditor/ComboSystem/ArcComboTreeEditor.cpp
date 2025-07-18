/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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

#include "ComboSystem/ArcComboTreeEditor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraphUtilities.h"
#include "GraphEditor.h"
#include "GraphEditorActions.h"
#include "IStructureDetailsView.h"
#include "Rendering/DrawElements.h"
#include "SNodePanel.h"
#include "SoundCueGraphEditorCommands.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"
#include "Widgets/Docking/SDockTab.h"

#include "ComboSystem/ArcComboGraphTreeNode.h"
#include "ComboSystem/ArcComboTreeGraphSchema.h"
#include "ComboSystem/ArcEdGraph_ComboTree.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "EdGraph/EdGraph.h"
#include "EditorSupportDelegates.h"
#include "FileHelpers.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailsView.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PropertyEditorModule.h"

#define LOCTEXT_NAMESPACE "FArcComboTreeEditor"

void SCCGraphPalette::Construct(const FArguments& InArgs
								, TWeakPtr<FArcComboTreeEditor> InAbilityTreeEditor)
{
	this->AbilityTreeEditor = InAbilityTreeEditor;

	this->ChildSlot[SNew(SBorder).Padding(2.0f).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))[
						SNew(SVerticalBox)

						// Content list
						+ SVerticalBox::Slot()[
							SNew(SOverlay) + SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Fill)[
								// SNullWidget::NullWidget
								SAssignNew(GraphActionMenu
									, SGraphActionMenu).OnActionDragged(this
									, &SCCGraphPalette::OnActionDragged).OnCreateWidgetForAction(this
									, &SCCGraphPalette::OnCreateWidgetForAction).OnCollectAllActions(this
									, &SCCGraphPalette::CollectAllActions).AutoExpandActionMenu(true)]]]];
}

void SCCGraphPalette::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	const UArcComboTreeGraphSchema* Schema = GetDefault<UArcComboTreeGraphSchema>();
	TArray<TSharedPtr<FEdGraphSchemaAction>> Actions;
	FGraphActionMenuBuilder ActionMenuBuilder;
	if (AbilityTreeEditor.IsValid())
	{
		TSharedPtr<SGraphEditor> GraphEditor = AbilityTreeEditor.Pin()->GetGraphEditor();
		if (GraphEditor.IsValid())
		{
			UEdGraph* Graph = GraphEditor->GetCurrentGraph();
			Schema->GetActionList(Actions
				, Graph
				, EMkSchemaActionType::All);
			for (TSharedPtr<FEdGraphSchemaAction> Action : Actions)
			{
				ActionMenuBuilder.AddAction(Action);
			}
		}
	}
	OutAllActions.Append(ActionMenuBuilder);
}

void SCCGraphPalette::Refresh()
{
	RefreshActionsList(true);
}

TSharedRef<SWidget> SCCGraphPalette::OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData)
{
	return SGraphPalette::OnCreateWidgetForAction(InCreateData);
}

FReply SCCGraphPalette::OnActionDragged(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions
										, const FPointerEvent& MouseEvent)
{
	return SGraphPalette::OnActionDragged(InActions
		, MouseEvent);
}

struct FArcComboTreeEditorTabs
{
	static const FName DetailsID;
	static const FName ActionsID;
	static const FName ViewportID;
};

const FName FArcComboTreeEditorTabs::DetailsID(TEXT("Details"));
const FName FArcComboTreeEditorTabs::ViewportID(TEXT("Viewport"));
const FName FArcComboTreeEditorTabs::ActionsID(TEXT("Actions"));

FName FArcComboTreeEditor::GetToolkitFName() const
{
	return FName("AbilityTreeEditor");
}

FText FArcComboTreeEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AbilityTreeEditorAppLabel"
		, "Ability Tree Editor");
}

FText FArcComboTreeEditor::GetToolkitName() const
{
	const bool bDirtyState = TreeBeingEdited->GetOutermost()->IsDirty();

	FFormatNamedArguments Args;
	Args.Add(TEXT("AbilityTreeName")
		, FText::FromString(TreeBeingEdited->GetName()));
	Args.Add(TEXT("DirtyState")
		, bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("AbilityTreeEditorAppLabel"
			, "{AbilityTreeName}{DirtyState}")
		, Args);
}

FString FArcComboTreeEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("AbilityTreeEditor");
}

void FArcComboTreeEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent
										   , FProperty* PropertyThatChanged)
{
	GraphEditor->NotifyGraphChanged();
}

FLinearColor FArcComboTreeEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

TSharedRef<SDockTab> FArcComboTreeEditor::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab).Label(LOCTEXT("AbilityGraph"
			   , "Ability Graph")).TabColorScale(GetTabColorScale())[GraphEditor.ToSharedRef()];
}

void FArcComboTreeEditor::InitAbilityTreeEditor(const EToolkitMode::Type Mode
												, const TSharedPtr<class IToolkitHost>& InitToolkitHost
												, UArcComboGraph* PropData)
{
	// Initialize the asset editor and spawn nothing (dummy layout)

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	AssetEditorSubsystem->CloseAllEditorsForAsset(PropData);

	TreeBeingEdited = PropData;

	if (!TreeBeingEdited->ATGraph)
	{
		UArcEdGraph_ComboTree* AbilityTreeGraph = NewObject<UArcEdGraph_ComboTree>(TreeBeingEdited
			, UArcEdGraph_ComboTree::StaticClass()
			, NAME_None
			, RF_Transactional);
		AbilityTreeGraph->InitializeGraph(PropData);
		TreeBeingEdited->ATGraph = AbilityTreeGraph;
	}

	GraphEditor = CreateGraphEditorWidget(TreeBeingEdited->ATGraph.Get());

	// Default layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout(ComboTreePropEditor)->
			AddArea(FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)->Split(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.8f)->Split(
					FTabManager::NewStack()->SetSizeCoefficient(0.8f)->SetHideTabWell(true)->AddTab(
						FArcComboTreeEditorTabs::ViewportID
						, ETabState::OpenedTab))->Split(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.37f)->Split(
						FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.33f)->Split(
							FTabManager::NewStack()->SetSizeCoefficient(0.6f)->AddTab(FArcComboTreeEditorTabs::DetailsID
								, ETabState::OpenedTab)))->Split(
						FTabManager::NewStack()->SetSizeCoefficient(0.6f)->AddTab(FArcComboTreeEditorTabs::ActionsID
							, ETabState::OpenedTab)))));

	// Initialize the asset editor and spawn nothing (dummy layout)
	InitAssetEditor(Mode
		, InitToolkitHost
		, ComboTreeEditorAppName
		, StandaloneDefaultLayout
		,
		/*bCreateDefaultStandaloneMenu=*/true
		,
		/*bCreateDefaultToolbar=*/true
		, PropData);

	// Listen for graph changed event
	OnGraphChangedDelegateHandle = GraphEditor->GetCurrentGraph()->AddOnGraphChangedHandler(
		FOnGraphChanged::FDelegate::CreateRaw(this
			, &FArcComboTreeEditor::OnGraphChanged));
	bGraphStateChanged = true;

	ShowObjectDetails(TreeBeingEdited);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

TSharedRef<SDockTab> FArcComboTreeEditor::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(
		"PropertyEditor");
	const FDetailsViewArgs DetailsViewArgs(false
		, false
		, true
		, FDetailsViewArgs::HideNameArea
		, true
		, this);
	TSharedPtr<IDetailsView> PropertyEditorRef = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	PropertyEditor = PropertyEditorRef;

	// Spawn the tab
	SAssignNew(DetailsTab
		, SDockTab).Label(LOCTEXT("DetailsTab_Title"
		, "Details"))[PropertyEditorRef.ToSharedRef()];

	return DetailsTab.ToSharedRef();
}

TSharedRef<SDockTab> FArcComboTreeEditor::SpawnTab_Actions(const FSpawnTabArgs& Args)
{
	ActionPalette = SNew(SCCGraphPalette
		, SharedThis(this));

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab).Icon(FAppStyle::GetBrush("Kismet.Tabs.Palette")).Label(LOCTEXT(
										  "ActionsPaletteTitle"
										  , "Ability Tree nodes"))[SNew(SBox).AddMetaData<FTagMetaData>(
																	   FTagMetaData(TEXT("Ability Tree nodes")))[
																	   ActionPalette.ToSharedRef()]];

	return SpawnedTab;
}

void FArcComboTreeEditor::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();

	AddToolbarExtender(ToolbarExtender);
}

TSharedRef<SGraphEditor> FArcComboTreeEditor::CreateGraphEditorWidget(UEdGraph* InGraph)
{
	// Create the appearance info
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText"
		, "Ability Tree");

	GraphEditorCommands = MakeShareable(new FUICommandList);
	{
		GraphEditorCommands->MapAction(FGenericCommands::Get().SelectAll
			, FExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::SelectAllNodes)
			, FCanExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::CanSelectAllNodes));

		GraphEditorCommands->MapAction(FGenericCommands::Get().Delete
			, FExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::DeleteSelectedNodes)
			, FCanExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::CanDeleteNodes));

		GraphEditorCommands->MapAction(FGenericCommands::Get().Copy
			, FExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::CopySelectedNodes)
			, FCanExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::CanCopyNodes));

		GraphEditorCommands->MapAction(FGenericCommands::Get().Paste
			, FExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::PasteNodes)
			, FCanExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::CanPasteNodes));

		GraphEditorCommands->MapAction(FGenericCommands::Get().Cut
			, FExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::CutSelectedNodes)
			, FCanExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::CanCutNodes));

		GraphEditorCommands->MapAction(FGenericCommands::Get().Duplicate
			, FExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::DuplicateNodes)
			, FCanExecuteAction::CreateSP(this
				, &FArcComboTreeEditor::CanDuplicateNodes));
	}

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this
		, &FArcComboTreeEditor::OnSelectedNodesChanged);

	// Make title bar
	TSharedRef<SWidget> TitleBarWidget =
			SNew(SBorder).BorderImage(FAppStyle::GetBrush(TEXT("Graph.TitleBackground"))).HAlign(HAlign_Fill)[
				SNew(SHorizontalBox) + SHorizontalBox::Slot().HAlign(HAlign_Center).FillWidth(1.f)[
					SNew(STextBlock).Text(LOCTEXT("UpdateGraphLabel"
						, "AbilityTree Graph")).TextStyle(FAppStyle::Get()
						, TEXT("GraphBreadcrumbButtonText"))]];

	TSharedRef<SGraphEditor> _GraphEditor = SNew(SGraphEditor_AbilityTree
		, SharedThis(this)).IsEmpty(true).AdditionalCommands(GraphEditorCommands).Appearance(AppearanceInfo)
						   //.TitleBar(TitleBarWidget)
						   .GraphToEdit(InGraph).GraphEvents(InEvents);

	return _GraphEditor;
}

void FArcComboTreeEditor::SelectAllNodes()
{
	GraphEditor->SelectAllNodes();
}

bool FArcComboTreeEditor::CanSelectAllNodes() const
{
	return GraphEditor.IsValid();
}

void FArcComboTreeEditor::DeleteSelectedNodes()
{
	DetailsTab->SetContent(PropertyEditor.ToSharedRef());
	TArray<UEdGraphNode*> NodesToDelete;
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		NodesToDelete.Add(CastChecked<UEdGraphNode>(*NodeIt));
	}

	DeleteNodes(NodesToDelete);
}

bool FArcComboTreeEditor::CanDeleteNode(class UEdGraphNode* Node)
{
	bool CanDelete = true;

	return CanDelete;
}

void FArcComboTreeEditor::DeleteNodes(const TArray<class UEdGraphNode*>& NodesToDelete)
{
	if (NodesToDelete.Num() > 0)
	{
		for (int32 Index = 0; Index < NodesToDelete.Num(); ++Index)
		{
			if (!CanDeleteNode(NodesToDelete[Index]))
			{
				continue;
			}

			NodesToDelete[Index]->DestroyNode();
		}
	}
}

void FArcComboTreeEditor::DeleteSelectedDuplicatableNodes()
{
	// Cache off the old selection
	const FGraphPanelSelectionSet OldSelectedNodes = GraphEditor->GetSelectedNodes();

	// Clear the selection and only select the nodes that can be duplicated
	FGraphPanelSelectionSet RemainingNodes;
	GraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(OldSelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if ((Node != NULL) && Node->CanDuplicateNode())
		{
			GraphEditor->SetNodeSelection(Node
				, true);
		}
		else
		{
			RemainingNodes.Add(Node);
		}
	}

	// Delete the duplicatable nodes
	DeleteSelectedNodes();

	// Reselect whatever is left from the original selection after the deletion
	GraphEditor->ClearSelectionSet();

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(RemainingNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			GraphEditor->SetNodeSelection(Node
				, true);
		}
	}
}

bool FArcComboTreeEditor::CanDeleteNodes() const
{
	return true;
}

void FArcComboTreeEditor::CopySelectedNodes()
{
	// Export the selected nodes and place the text on the clipboard

	FGraphPanelSelectionSet SelectedNodes = FGraphPanelSelectionSet();

	FString ExportedText;
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
			Node->PrepareForCopying();
		}
	}

	FEdGraphUtilities::ExportNodesToText(SelectedNodes
		, /*out*/ ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter))
		{
		}
	}
}

bool FArcComboTreeEditor::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	for (FGraphPanelSelectionSet::TConstIterator SelectedIter(SelectedNodes); SelectedIter; ++SelectedIter)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*SelectedIter);
		if ((Node != NULL) && Node->CanDuplicateNode())
		{
			return true;
		}
	}
	return false;
}

void FArcComboTreeEditor::PasteNodes()
{
	PasteNodesHere(GraphEditor->GetPasteLocation());
}

void FArcComboTreeEditor::PasteNodesHere(const FVector2D& Location)
{
	// Clear the selection set (newly pasted stuff will be selected)
	GraphEditor->ClearSelectionSet();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes
	if (!TreeBeingEdited)
	{
		return;
	}
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(TreeBeingEdited->ATGraph.Get()
		, TextToImport
		, /*out*/ PastedNodes);

	// Average position of nodes so we can move them while still maintaining relative
	// distances to each other
	FVector2D AvgNodePosition(0.0f
		, 0.0f);

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;
		AvgNodePosition.X += Node->NodePosX;
		AvgNodePosition.Y += Node->NodePosY;
	}

	if (PastedNodes.Num() > 0)
	{
		float InvNumNodes = 1.0f / float(PastedNodes.Num());
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}

	for (TSet<UEdGraphNode*>::TIterator It(PastedNodes); It; ++It)
	{
		UEdGraphNode* Node = *It;

		// Select the newly pasted stuff
		GraphEditor->SetNodeSelection(Node
			, true);
		Node->NodePosX = (Node->NodePosX - AvgNodePosition.X) + Location.X;
		Node->NodePosY = (Node->NodePosY - AvgNodePosition.Y) + Location.Y;
		Node->SnapToGrid(SNodePanel::GetSnapGridSize());

		// Give new node a different Guid from the old one
		Node->CreateNewGuid();
	}

	// Update UI
	GraphEditor->NotifyGraphChanged();
}

bool FArcComboTreeEditor::CanPasteNodes() const
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(TreeBeingEdited->ATGraph.Get()
		, ClipboardContent);
}

void FArcComboTreeEditor::CutSelectedNodes()
{
	CopySelectedNodes();
	// Cut should only delete nodes that can be duplicated
	DeleteSelectedDuplicatableNodes();
}

bool FArcComboTreeEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FArcComboTreeEditor::DuplicateNodes()
{
	// Copy and paste current selection
	CopySelectedNodes();
	PasteNodes();
}

bool FArcComboTreeEditor::CanDuplicateNodes() const
{
	return CanCopyNodes();
}

void FArcComboTreeEditor::OnGraphChanged(const FEdGraphEditAction& Action)
{
	bGraphStateChanged = true;
}

void FArcComboTreeEditor::SaveAsset_Execute()
{
	UPackage* Package = TreeBeingEdited->GetOutermost();
	if (Package)
	{
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(Package);
		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave
			, false
			, false);
	}
}

void FArcComboTreeEditor::OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection)
{
	if (NewSelection.Num() == 1)
	{
		for (UObject* SelectedNode : NewSelection)
		{
			UArcComboGraphTreeNode* N = Cast<UArcComboGraphTreeNode>(SelectedNode);

			FArcComboNode* Node = TreeBeingEdited->FindNode(N->NodeId);

			FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(
				"PropertyEditor");

			TSharedPtr<FStructOnScope> StructOnScope = MakeShared<FStructOnScope>(FArcComboNode::StaticStruct()
				, (uint8*)Node);

			FDetailsViewArgs DetailsViewArgs(false);
			FStructureDetailsViewArgs StructureDetails;
			TSharedRef<class IStructureDetailsView> StructDetails = PropertyEditorModule.CreateStructureDetailView(
				DetailsViewArgs
				, StructureDetails
				, StructOnScope);
			GraphEditor->RefreshNode(*N);
			DetailsTab->SetContent(StructDetails->GetWidget().ToSharedRef());
		}
	}

	TArray<UObject*> SelectedObjects;
	for (UObject* Object : NewSelection)
	{
		SelectedObjects.Add(Object);

		UArcComboGraphTreeNode* Node = Cast<UArcComboGraphTreeNode>(Object);
		if (Node)
		{
			// LastSelectedNodes.Add(Node);
		}
	}

	if (SelectedObjects.Num() == 0)
	{
		PropertyEditor->SetObject(GetPropBeingEdited());
		DetailsTab->SetContent(PropertyEditor.ToSharedRef());
		return;
	}

	PropertyEditor->SetObjects(SelectedObjects);
}

FArcComboTreeEditor::~FArcComboTreeEditor()
{
	if (GraphEditor->GetCurrentGraph())
	{
		GraphEditor->GetCurrentGraph()->RemoveOnGraphChangedHandler(OnGraphChangedDelegateHandle);
	}
}

void FArcComboTreeEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManagerReg)
{
	WorkspaceMenuCategory = TabManagerReg->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_AbilityTreeEditor"
		, "Ability Tree Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(TabManagerReg);

	TabManagerReg->RegisterTabSpawner(FArcComboTreeEditorTabs::DetailsID
		, FOnSpawnTab::CreateSP(this
			, &FArcComboTreeEditor::SpawnTab_Details)).SetDisplayName(LOCTEXT("DetailsTabLabel"
		, "Details")).SetGroup(WorkspaceMenuCategoryRef).SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName()
		, "LevelEditor.Tabs.Details"));

	TabManagerReg->RegisterTabSpawner(FArcComboTreeEditorTabs::ViewportID
		, FOnSpawnTab::CreateSP(this
			, &FArcComboTreeEditor::SpawnTab_Viewport)).SetDisplayName(LOCTEXT("ViewportTab"
		, "Viewport")).SetGroup(WorkspaceMenuCategoryRef).SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName()
		, "LevelEditor.Tabs.Viewports"));

	TabManagerReg->RegisterTabSpawner(FArcComboTreeEditorTabs::ActionsID
		, FOnSpawnTab::CreateSP(this
			, &FArcComboTreeEditor::SpawnTab_Actions)).SetDisplayName(LOCTEXT("ActionsTabLabel"
		, "Actions")).SetGroup(WorkspaceMenuCategoryRef).SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName()
		, "LevelEditor.Tabs.Details"));
}

void FArcComboTreeEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManagerReg)
{
	FAssetEditorToolkit::UnregisterTabSpawners(TabManagerReg);

	TabManagerReg->UnregisterTabSpawner(FArcComboTreeEditorTabs::ViewportID);
	TabManagerReg->UnregisterTabSpawner(FArcComboTreeEditorTabs::ActionsID);
	TabManagerReg->UnregisterTabSpawner(FArcComboTreeEditorTabs::DetailsID);
}

void FArcComboTreeEditor::ShowObjectDetails(UObject* ObjectProperties)
{
	PropertyEditor->SetObject(ObjectProperties);
}

void SGraphEditor_AbilityTree::Construct(const FArguments& InArgs
										 , TWeakPtr<class FArcComboTreeEditor> InAbilityTreeEditor)
{
	AbilityTreeEditor = InAbilityTreeEditor;

	SGraphEditor::Construct(InArgs);
}

void SGraphEditor_AbilityTree::Tick(const FGeometry& AllottedGeometry
									, const double InCurrentTime
									, const float InDeltaTime)
{
	SGraphEditor::Tick(AllottedGeometry
		, InCurrentTime
		, InDeltaTime);
	SGraphEditor::GetViewLocation(ViewLocation
		, ZoomAmount);
}

int32 SGraphEditor_AbilityTree::OnPaint(const FPaintArgs& Args
										, const FGeometry& AllottedGeometry
										, const FSlateRect& MyCullingRect
										, FSlateWindowElementList& OutDrawElements
										, int32 LayerId
										, const FWidgetStyle& InWidgetStyle
										, bool bParentEnabled) const
{
	return SGraphEditor::OnPaint(Args
		, AllottedGeometry
		, MyCullingRect
		, OutDrawElements
		, LayerId - 1
		, InWidgetStyle
		, bParentEnabled);
}

#undef LOCTEXT_NAMESPACE
