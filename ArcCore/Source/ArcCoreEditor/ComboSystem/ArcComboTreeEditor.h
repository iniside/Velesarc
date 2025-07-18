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

#pragma once
#include "AbilitySystem/Combo/ArcComboComponent.h"
#include "AssetThumbnail.h"
#include "EditorUndoClient.h"
#include "GraphEditor.h"
#include "IDetailsView.h"
#include "Misc/NotifyHook.h"
#include "Tickable.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkit.h"

#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandInfo.h"
#include "SGraphPalette.h"

const FName ComboTreeEditorAppName = FName(TEXT("ComboTreeEditorAppV1"));
const FName ComboTreePropEditor = FName(TEXT("ComboTreePropEditor_Layout_v1"));

enum class ETTToolMode
{
	Find = 1
	, Modify = 2
	,
};

class UArcComboGraph;

class ARCCOREEDITOR_API SCCGraphPalette : public SGraphPalette
{
public:
	SLATE_BEGIN_ARGS(SCCGraphPalette)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs
				   , TWeakPtr<class FArcComboTreeEditor> InAbilityTreeEditor);

	virtual void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) override;

	virtual TSharedRef<SWidget> OnCreateWidgetForAction(FCreateWidgetForActionData* const InCreateData) override;

	virtual FReply OnActionDragged(const TArray<TSharedPtr<FEdGraphSchemaAction>>& InActions
								   , const FPointerEvent& MouseEvent) override;

	void Refresh();

protected:
	TWeakPtr<class FArcComboTreeEditor> AbilityTreeEditor;
};

class FArcComboTreeEditor
		: public FAssetEditorToolkit
		, public FNotifyHook
{
private:
	TSharedPtr<SDockTab> DetailsTab;

public:
	virtual ~FArcComboTreeEditor() override;

	// IToolkit interface
	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	// End of IToolkit interface

	// FAssetEditorToolkit
	virtual FName GetToolkitFName() const override;

	virtual FText GetBaseToolkitName() const override;

	virtual FText GetToolkitName() const override;

	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	virtual FString GetWorldCentricTabPrefix() const override;

	// End of FAssetEditorToolkit

	// FNotifyHook
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent
								  , FProperty* PropertyThatChanged) override;

	void InitAbilityTreeEditor(const EToolkitMode::Type Mode
							   , const TSharedPtr<class IToolkitHost>& InitToolkitHost
							   , UArcComboGraph* PropData);

	UArcComboGraph* GetPropBeingEdited() const
	{
		return TreeBeingEdited;
	}

	FORCEINLINE TSharedPtr<SGraphEditor> GetGraphEditor() const
	{
		return GraphEditor;
	}

	void ShowObjectDetails(UObject* ObjectProperties);

protected:
	TSharedRef<class SGraphEditor> CreateGraphEditorWidget(UEdGraph* InGraph);

	// Select every node in the graph
	void SelectAllNodes();

	// Whether we can select every node
	bool CanSelectAllNodes() const;

	// Deletes all the selected nodes
	void DeleteSelectedNodes();

	bool CanDeleteNode(class UEdGraphNode* Node);

	// Delete an array of Material Graph Nodes and their corresponding
	// expressions/comments
	void DeleteNodes(const TArray<class UEdGraphNode*>& NodesToDelete);

	// Delete only the currently selected nodes that can be duplicated
	void DeleteSelectedDuplicatableNodes();

	// Whether we are able to delete the currently selected nodes
	bool CanDeleteNodes() const;

	// Copy the currently selected nodes
	void CopySelectedNodes();

	// Whether we are able to copy the currently selected nodes
	bool CanCopyNodes() const;

	// Paste the contents of the clipboard
	void PasteNodes();

	virtual bool CanPasteNodes() const;

	virtual void PasteNodesHere(const FVector2D& Location);

	// Cut the currently selected nodes
	void CutSelectedNodes();

	// Whether we are able to cut the currently selected nodes
	bool CanCutNodes() const;

	// Duplicate the currently selected nodes
	void DuplicateNodes();

	// Whether we are able to duplicate the currently selected nodes
	bool CanDuplicateNodes() const;

	void OnGraphChanged(const FEdGraphEditAction& Action);

protected:
	// Called when "Save" is clicked for this asset
	virtual void SaveAsset_Execute() override;

	// Called when the selection changes in the GraphEditor
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);

	TSharedPtr<SGraphEditor> GraphEditor;
	TSharedPtr<SCCGraphPalette> ActionPalette;
	TSharedPtr<FUICommandList> GraphEditorCommands;
	TSharedPtr<IDetailsView> PropertyEditor;

	UArcComboGraph* TreeBeingEdited = nullptr;

	TSharedRef<SDockTab> SpawnTab_Viewport(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args);

	TSharedRef<SDockTab> SpawnTab_Actions(const FSpawnTabArgs& Args);

	// Handle to the registered OnGraphChanged delegate.
	FDelegateHandle OnGraphChangedDelegateHandle;

	bool bGraphStateChanged;
	TArray<class UCCNode_Group*> LastSelectedGroupNodes;

private:
	// Builds the toolbar. Calls FillToolbar
	void ExtendToolbar();
};

class SGraphEditor_AbilityTree : public SGraphEditor
{
public:
	void Construct(const FArguments& InArgs
				   , TWeakPtr<class FArcComboTreeEditor> InAbilityTreeEditor);

	// SWidget implementation
	virtual void Tick(const FGeometry& AllottedGeometry
					  , const double InCurrentTime
					  , const float InDeltaTime) override;

	virtual int32 OnPaint(const FPaintArgs& Args
						  , const FGeometry& AllottedGeometry
						  , const FSlateRect& MyCullingRect
						  , FSlateWindowElementList& OutDrawElements
						  , int32 LayerId
						  , const FWidgetStyle& InWidgetStyle
						  , bool bParentEnabled) const override;

	// End SWidget implementation

protected:
	TWeakPtr<FArcComboTreeEditor> AbilityTreeEditor;

private:
	FVector2D ViewLocation;
	float ZoomAmount;
};
