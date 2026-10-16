// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GraphEditor.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FUICommandList;
class IDetailsView;
class IStructureDetailsView;
class SGraphEditor;
class UArcHeightmapEdGraph;
class UArcHeightmapGraph;
struct FPropertyChangedEvent;

/**
 * Top-level Slate widget hosting the heightmap graph canvas + node details panel.
 *
 * Builds a transient UArcHeightmapEdGraph mirror of the source UArcHeightmapGraph asset
 * (via ArcHeightmapGraphBridge) and drives the asset round-trip: graph edits (delete /
 * drag / connect) and details-panel commits both write back into the asset, keyed by
 * FArcHeightmapNode::NodeId. The mirror is rooted for its lifetime so the GC never
 * collects it while SGraphEditor holds a raw pointer.
 *
 * The details panel never binds directly to live FInstancedStruct memory; it edits an
 * OWNED FStructOnScope copy and commits the copy back under a transaction.
 */
class SArcHeightmapGraphView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcHeightmapGraphView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UArcHeightmapGraph* InAsset);

	virtual ~SArcHeightmapGraphView() override;

	/** Write the current mirror state back into the asset (called by the toolkit on Save). */
	void FlushToAsset();

	/** The combined details panel (asset resolution/coverage on top, selected-node struct below), hosted in its own toolkit tab. */
	TSharedRef<SWidget> GetDetailsWidget() const { return DetailsContainer.ToSharedRef(); }

private:

	// ---- Handlers ---------------------------------------------------------------

	/** UArcHeightmapEdGraph::OnGraphEdited: native delete/drag/connect -> write-back. */
	void HandleGraphEdited();

	/** Delete command: transactional node destruction + write-back. */
	void DeleteSelectedNodes();

	/** Delete command CanExecute: true if any selected node is user-deletable. */
	bool CanDeleteSelectedNodes() const;

	/** SGraphEditor selection-changed: load an OWNED copy of the node struct into details. */
	void HandleSelectionChanged(const FGraphPanelSelectionSet& NewSelection);

	/** Clear the details view + selected struct scope. */
	void ClearDetails();

	/** Details-panel commit: copy the owned scope back into the asset under a transaction. */
	void HandleFinishedChanging(const FPropertyChangedEvent& PropertyChangedEvent);

	/** Defensive guard against direct edits to the asset: rebuild the mirror. */
	void HandleAssetPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent);

	// ---- Data members -----------------------------------------------------------

	/** The transient editor-graph mirror; rooted in Construct, unrooted in dtor. */
	TObjectPtr<UArcHeightmapEdGraph> EdGraph = nullptr;

	/** The source data asset this view mirrors. */
	TWeakObjectPtr<UArcHeightmapGraph> Asset;

	/** The graph editor widget. */
	TSharedPtr<SGraphEditor> GraphEditor;

	/**
	 * Command list mapping Delete -> DeleteSelectedNodes, handed to SGraphEditor as
	 * AdditionalCommands. MUST be retained as a member: FUICommandList::Append stores
	 * appended lists as weak parents, so a local would be destroyed after Construct and
	 * the Delete binding would become unreachable (keyboard + context menu both break).
	 */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/** Structure details view shown beside the canvas. */
	TSharedPtr<IStructureDetailsView> StructDetailsView;

	/** Asset-level details view exposing only DefaultResolution + PreviewCoverage. */
	TSharedPtr<IDetailsView> AssetDetailsView;

	/** Combined container (asset details on top, node struct details below) hosted in the Details tab. */
	TSharedPtr<SWidget> DetailsContainer;

	/** OWNED copy of the selected node's struct; never aliases live asset memory. */
	TSharedPtr<FStructOnScope> SelectedStructScope;

	/** NodeId of the struct currently mirrored into SelectedStructScope. */
	FGuid SelectedNodeId;

	/** Re-entrancy guard for the write-back path. */
	bool bWritingBack = false;

	/** Handle for the FCoreUObjectDelegates::OnObjectPropertyChanged registration. */
	FDelegateHandle OnObjectPropertyChangedHandle;
};
