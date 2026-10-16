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

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GraphEditor.h"
#include "RecipeGraph/ArcRecipeGraphData.h"
#include "RecipeGraph/ArcRecipeGraphLayout.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class IStructureDetailsView;
class FStructOnScope;
class SGraphEditor;
class UEdGraphNode;
class UArcRecipeGraph;
class UArcRecipeGraphNode;
class UArcRecipeGraphAddIngredientOptions;

/**
 * Top-level Slate widget for the ArcCraft recipe dependency graph tab.
 *
 * Hosts a toolbar (Refresh, Show All, Search) above an SGraphEditor displaying
 * the full UArcRecipeGraph. Responds to graph-delegate callbacks (OnIsolateRecipe,
 * OnShowAll, OnOpenAsset) forwarded from context-menu actions in UArcRecipeGraphSchema.
 */
class SArcRecipeGraphView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcRecipeGraphView) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual ~SArcRecipeGraphView() override;

private:

	/** Rebuild the graph from all project recipe / item assets. */
	void RebuildFull();

	/** Repopulate and re-layout Graph from pure data, then request a deferred zoom. */
	void RepopulateFrom(const FArcRecipeGraphData& Data);

	/** Delegate: narrow the view to the subgraph rooted at RootRecipe. */
	void HandleIsolate(FPrimaryAssetId RootRecipe);

	/** Delegate: restore the full graph. */
	void HandleShowAll();

	/** Delegate: open the asset editor for the given primary asset. */
	void HandleOpenAsset(FPrimaryAssetId AssetId);

	/** Active-timer callback: calls ZoomToFit once the panel is built, then stops. */
	EActiveTimerReturnType HandleDeferredZoomToFit(double InCurrentTime, float InDeltaTime);

	/** Toolbar combo box: called when the user picks a new layout mode. */
	void HandleLayoutModeChanged(TSharedPtr<EArcRecipeGraphLayoutMode> NewMode, ESelectInfo::Type SelectInfo);

	/** Return the display label for a layout mode (used in both the combo items and the button content). */
	static FText GetLayoutModeLabel(EArcRecipeGraphLayoutMode Mode);

	/** SGraphEditor selection-changed: drive chain highlight + details panel. */
	void HandleSelectionChanged(const FGraphPanelSelectionSet& NewSelection);

	/** SGraphEditor node double-click: open the underlying asset (Recipe/RawItem). */
	void HandleNodeDoubleClicked(UEdGraphNode* Node);

	/** TagGroup member-row single-click: show that item in the details panel. */
	void HandleMemberSelected(FPrimaryAssetId MemberId);

	/** Show a read-only copy of a recipe or item record; an invalid id clears it. */
	void ShowRecordInDetails(FPrimaryAssetId RecordId);

	/** Asset manager record change: schedules one rebuild when a recipe or item record changed. */
	void HandleJsonRecordChanged(const FPrimaryAssetId& Id);

	EActiveTimerReturnType HandleDeferredRebuild(double InCurrentTime, float InDeltaTime);

	/** Dim every node not in the selected node's transitive dependency chain. Null clears all. */
	void UpdateChainHighlight(UArcRecipeGraphNode* Selected);

	// ---- Authoring callbacks ------------------------------------------------

	/** Delegate: create a new item record file under a content root, add to graph. */
	void HandleCreateItem();

	/** Delegate: create a new recipe record file under a content root, add to graph. */
	void HandleCreateRecipe();

	/** Delegate: begin adding a dependency from Source -> TargetRecipe (deferred). */
	void HandleAddDependency(UArcRecipeGraphNode* Source, UArcRecipeGraphNode* TargetRecipe);

	/** Active-timer: apply the deferred dependency once the engine has finished with the dropped pins. */
	EActiveTimerReturnType ApplyPendingDependency(double InCurrentTime, float InDeltaTime);

	/** Insert PinnedItemIds as orphan RawItem nodes into Graph after PopulateGraph. */
	void InjectPinnedItems();

	/** Show a modal details-view dialog for Options. Returns true if user confirmed. */
	bool PromptAddIngredient(UArcRecipeGraphAddIngredientOptions* Options);

	// ---- Data members -------------------------------------------------------

	/** The UEdGraph object; kept alive via AddToRoot / RemoveFromRoot. */
	TObjectPtr<UArcRecipeGraph> Graph = nullptr;

	/** The graph editor widget. */
	TSharedPtr<SGraphEditor> GraphEditor;

	/** Read-only record panel shown beside the graph, driven by selection. */
	TSharedPtr<IStructureDetailsView> RecordDetailsView;
	TSharedPtr<FStructOnScope> RecordSnapshot;

	FDelegateHandle JsonRecordChangedHandle;
	bool bPendingRebuild = false;

	/** Full project graph data; retained so focused views can be derived cheaply. */
	FArcRecipeGraphData FullData;

	/** True while a deferred zoom-to-fit timer is pending. */
	bool bPendingZoomToFit = false;

	/** Currently active layout mode; persisted in EditorPerProjectIni. */
	EArcRecipeGraphLayoutMode LayoutMode = EArcRecipeGraphLayoutMode::MinCrossings;

	/** Option items backing the layout mode combo box; populated before SNew(SComboBox). */
	TArray<TSharedPtr<EArcRecipeGraphLayoutMode>> LayoutModeOptions;

	/** Item record ids that were created via "Create Item" and must survive a RebuildFull. */
	TArray<FPrimaryAssetId> PinnedItemIds;

	/** Captured state for a pending HandleAddDependency call; applied one tick later. */
	struct FPendingDependency
	{
		bool bValid = false;
		EArcRecipeGraphNodeType SourceType = EArcRecipeGraphNodeType::Recipe;
		FPrimaryAssetId SourceId;
		FGameplayTagContainer SourceGroupTags;
		FPrimaryAssetId TargetRecipeId;
	};
	FPendingDependency PendingDependency;
};
