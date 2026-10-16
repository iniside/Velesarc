// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/Editor/SArcHeightmapGraphView.h"

#include "TerrainTool/Graph/ArcHeightmapGraph.h"
#include "TerrainTool/Graph/ArcHeightmapNodes.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraph.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraphNode.h"
#include "TerrainTool/Graph/Editor/ArcHeightmapGraphBridge.h"

#include "EdGraph/EdGraphNode.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/UICommandList.h"
#include "GraphEditor.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SArcHeightmapGraphView"

void SArcHeightmapGraphView::Construct(const FArguments& InArgs, UArcHeightmapGraph* InAsset)
{
	Asset = InAsset;

	// Build the transient editor-graph mirror and root it so SGraphEditor's raw
	// pointer never dangles (RecipeGraph / SPhysicsAssetGraph idiom).
	EdGraph = NewObject<UArcHeightmapEdGraph>(GetTransientPackage(), NAME_None, RF_Transactional);
	EdGraph->SourceAsset = InAsset;
	EdGraph->AddToRoot();

	if (InAsset != nullptr)
	{
		ArcHeightmapGraphBridge::BuildEdGraphFromAsset(*InAsset, *EdGraph);
	}

	// Native delete / drag / connect all route here via NotifyGraphChanged().
	EdGraph->OnGraphEdited.AddSP(this, &SArcHeightmapGraphView::HandleGraphEdited);

	// Reviewer #8 (direct-edit guard): the toolkit hosts only this widget, so the
	// canvas is the sole authoring path. Still register defensively so a direct
	// asset edit (e.g. another tool) rebuilds the mirror instead of going stale.
	OnObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(
		this, &SArcHeightmapGraphView::HandleAssetPropertyChanged);

	// Selection drives the details panel; commands route Delete to our write-back.
	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(
		this, &SArcHeightmapGraphView::HandleSelectionChanged);

	// Reviewer #4: SGraphEditor does NOT auto-route Delete to our write-back —
	// map it explicitly onto a transactional handler.
	// MUST be a retained member: FUICommandList::Append stores AdditionalCommands as a
	// weak parent list, so a local would die after Construct and the Delete binding would
	// become unreachable (keyboard + node context menu both silently fail).
	GraphEditorCommands = MakeShared<FUICommandList>();
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SArcHeightmapGraphView::DeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &SArcHeightmapGraphView::CanDeleteSelectedNodes));

	SAssignNew(GraphEditor, SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.GraphToEdit(EdGraph)
		.GraphEvents(Events)
		.IsEditable(true);

	// Structure details view: edits an OWNED FStructOnScope, never live asset memory.
	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	FStructureDetailsViewArgs StructArgs;
	StructDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsArgs, StructArgs, nullptr);
	StructDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(
		this, &SArcHeightmapGraphView::HandleFinishedChanging);

	// Asset-level details view: exposes ONLY DefaultResolution + PreviewCoverage so the bake
	// output resolution (and preview footprint) are editable, without surfacing Nodes (authored
	// via the canvas) or internal bookkeeping (BakedField / NodePositions).
	FDetailsViewArgs AssetDetailsArgs;
	AssetDetailsArgs.bAllowSearch = false;
	AssetDetailsArgs.bHideSelectionTip = true;
	AssetDetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	AssetDetailsView = PropertyEditorModule.CreateDetailView(AssetDetailsArgs);
	AssetDetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda(
		[](const FPropertyAndParent& PropertyAndParent) -> bool
		{
			const FName PropertyName = PropertyAndParent.Property.GetFName();
			return PropertyName == GET_MEMBER_NAME_CHECKED(UArcHeightmapGraph, DefaultResolution)
				|| PropertyName == GET_MEMBER_NAME_CHECKED(UArcHeightmapGraph, PreviewCoverage);
		}));
	AssetDetailsView->SetObject(InAsset);

	DetailsContainer =
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			AssetDetailsView.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			StructDetailsView->GetWidget().ToSharedRef()
		];

	ChildSlot
	[
		GraphEditor.ToSharedRef()
	];
}

SArcHeightmapGraphView::~SArcHeightmapGraphView()
{
	if (OnObjectPropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(OnObjectPropertyChangedHandle);
		OnObjectPropertyChangedHandle.Reset();
	}

	if (EdGraph != nullptr)
	{
		EdGraph->RemoveFromRoot();
	}
}

void SArcHeightmapGraphView::FlushToAsset()
{
	if (EdGraph != nullptr && Asset.IsValid())
	{
		ArcHeightmapGraphBridge::WriteAssetFromEdGraph(*EdGraph, *Asset.Get());
	}
}

void SArcHeightmapGraphView::HandleGraphEdited()
{
	// Guard the OnObjectPropertyChanged path: WriteAsset mutates Asset.Nodes, which
	// would otherwise re-enter HandleAssetPropertyChanged and rebuild the mirror.
	if (bWritingBack)
	{
		return;
	}
	if (EdGraph == nullptr || !Asset.IsValid())
	{
		return;
	}

	bWritingBack = true;
	ArcHeightmapGraphBridge::WriteAssetFromEdGraph(*EdGraph, *Asset.Get());
	bWritingBack = false;
}

void SArcHeightmapGraphView::DeleteSelectedNodes()
{
	if (!GraphEditor.IsValid() || EdGraph == nullptr || !Asset.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(NSLOCTEXT("ArcHeightmapGraph", "DeleteNodes", "Delete Nodes"));

	// Two-pass: snapshot the deletable selection BEFORE destroying any node, so we
	// never mutate the selection set while iterating it.
	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	TArray<UEdGraphNode*> NodesToDelete;
	NodesToDelete.Reserve(SelectedNodes.Num());
	for (UObject* SelectedObject : SelectedNodes)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(SelectedObject);
		if (Node != nullptr && Node->CanUserDeleteNode())
		{
			NodesToDelete.Add(Node);
		}
	}

	if (NodesToDelete.Num() == 0)
	{
		// Nothing user-deletable (e.g. only the _Output node) — drop the transaction.
		Transaction.Cancel();
		return;
	}

	bWritingBack = true;
	for (UEdGraphNode* Node : NodesToDelete)
	{
		Node->Modify();
		EdGraph->Modify();
		Node->DestroyNode();
	}

	GraphEditor->ClearSelectionSet();
	ArcHeightmapGraphBridge::WriteAssetFromEdGraph(*EdGraph, *Asset.Get());
	bWritingBack = false;

	// A deleted node may have been the one mirrored into the details panel.
	ClearDetails();
}

bool SArcHeightmapGraphView::CanDeleteSelectedNodes() const
{
	if (!GraphEditor.IsValid())
	{
		return false;
	}

	const FGraphPanelSelectionSet SelectedNodes = GraphEditor->GetSelectedNodes();
	for (UObject* SelectedObject : SelectedNodes)
	{
		const UEdGraphNode* Node = Cast<UEdGraphNode>(SelectedObject);
		if (Node != nullptr && Node->CanUserDeleteNode())
		{
			return true;
		}
	}
	return false;
}

void SArcHeightmapGraphView::HandleSelectionChanged(const FGraphPanelSelectionSet& NewSelection)
{
	// Reviewer #6: never bind FStructOnScope to live FInstancedStruct memory — start
	// by clearing, then load an OWNED copy of the single selected node's struct.
	ClearDetails();

	if (NewSelection.Num() != 1 || !Asset.IsValid())
	{
		return;
	}

	UArcHeightmapEdGraphNode* SelectedNode = nullptr;
	for (UObject* SelectedObject : NewSelection)
	{
		SelectedNode = Cast<UArcHeightmapEdGraphNode>(SelectedObject);
	}
	if (SelectedNode == nullptr)
	{
		return;
	}

	UArcHeightmapGraph* AssetPtr = Asset.Get();
	for (const FInstancedStruct& Entry : AssetPtr->Nodes)
	{
		const UScriptStruct* ScriptStruct = Entry.GetScriptStruct();
		if (ScriptStruct == nullptr)
		{
			continue;
		}

		const FArcHeightmapNode& NodeData = Entry.Get<FArcHeightmapNode>();
		if (NodeData.NodeId != SelectedNode->NodeId)
		{
			continue;
		}

		// OWNED copy: allocate a scope of the same type and deep-copy the struct in.
		SelectedStructScope = MakeShared<FStructOnScope>(ScriptStruct);
		ScriptStruct->CopyScriptStruct(SelectedStructScope->GetStructMemory(), Entry.GetMemory());
		SelectedNodeId = SelectedNode->NodeId;

		StructDetailsView->SetStructureData(SelectedStructScope);
		return;
	}
}

void SArcHeightmapGraphView::ClearDetails()
{
	if (StructDetailsView.IsValid())
	{
		StructDetailsView->SetStructureData(nullptr);
	}
	SelectedStructScope.Reset();
	SelectedNodeId.Invalidate();
}

void SArcHeightmapGraphView::HandleFinishedChanging(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!SelectedStructScope.IsValid() || !SelectedNodeId.IsValid() || !Asset.IsValid())
	{
		return;
	}

	const UScriptStruct* ScopeStruct = Cast<const UScriptStruct>(SelectedStructScope->GetStruct());
	if (ScopeStruct == nullptr)
	{
		return;
	}

	UArcHeightmapGraph* AssetPtr = Asset.Get();
	for (FInstancedStruct& Entry : AssetPtr->Nodes)
	{
		const UScriptStruct* EntryStruct = Entry.GetScriptStruct();
		if (EntryStruct == nullptr)
		{
			continue;
		}

		const FArcHeightmapNode& NodeData = Entry.Get<FArcHeightmapNode>();
		if (NodeData.NodeId != SelectedNodeId)
		{
			continue;
		}

		FScopedTransaction Transaction(
			NSLOCTEXT("ArcHeightmapGraph", "EditNode", "Edit Heightmap Node"));

		bWritingBack = true;
		AssetPtr->Modify();
		// Copy the OWNED scope back into the live entry (same struct type).
		EntryStruct->CopyScriptStruct(Entry.GetMutableMemory(), SelectedStructScope->GetStructMemory());
		AssetPtr->MarkPackageDirty();
		bWritingBack = false;
		return;
	}
}

void SArcHeightmapGraphView::HandleAssetPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (bWritingBack || !Asset.IsValid() || EdGraph == nullptr)
	{
		return;
	}
	if (Object != Asset.Get())
	{
		return;
	}

	// DefaultResolution / PreviewCoverage are edited live in the Details tab and do NOT affect
	// node topology — rebuilding the mirror here would clobber the current node selection (and
	// the OWNED struct scope the user may be editing) on every keystroke. Skip the rebuild for them.
	const FName ChangedName = PropertyChangedEvent.GetPropertyName();
	const FName ChangedMemberName = PropertyChangedEvent.GetMemberPropertyName();
	if (ChangedName == GET_MEMBER_NAME_CHECKED(UArcHeightmapGraph, DefaultResolution)
		|| ChangedName == GET_MEMBER_NAME_CHECKED(UArcHeightmapGraph, PreviewCoverage)
		|| ChangedMemberName == GET_MEMBER_NAME_CHECKED(UArcHeightmapGraph, DefaultResolution)
		|| ChangedMemberName == GET_MEMBER_NAME_CHECKED(UArcHeightmapGraph, PreviewCoverage))
	{
		return;
	}

	// The asset changed out from under us (direct edit) — rebuild the mirror.
	// Two-pass: snapshot then destroy, never mutate Nodes while iterating it.
	TArray<UEdGraphNode*> ExistingNodes = EdGraph->Nodes;
	for (UEdGraphNode* Node : ExistingNodes)
	{
		if (Node != nullptr)
		{
			Node->DestroyNode();
		}
	}
	EdGraph->Nodes.Empty();

	ArcHeightmapGraphBridge::BuildEdGraphFromAsset(*Asset.Get(), *EdGraph);

	if (GraphEditor.IsValid())
	{
		GraphEditor->NotifyGraphChanged();
	}

	ClearDetails();
}

#undef LOCTEXT_NAMESPACE
