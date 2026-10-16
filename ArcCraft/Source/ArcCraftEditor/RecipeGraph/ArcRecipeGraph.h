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
#include "EdGraph/EdGraph.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcRecipeGraph.generated.h"

/** Fired when the user selects "Isolate Dependencies" from the node context menu. */
DECLARE_DELEGATE_OneParam(FArcOnIsolateRecipe, FPrimaryAssetId);

/** Fired when the user selects "Show All" from the node context menu. */
DECLARE_DELEGATE(FArcOnShowAll);

/** Fired when the user selects "Open Recipe Asset" from the node context menu. */
DECLARE_DELEGATE_OneParam(FArcOnOpenAsset, FPrimaryAssetId);

/** Fired when the user single-clicks a TagGroup member row. */
DECLARE_DELEGATE_OneParam(FArcOnSelectMember, FPrimaryAssetId);

/** Fired when the user invokes "Create Item" or "Create Recipe" from the graph background menu. */
DECLARE_DELEGATE(FArcOnCreateAsset);

/** Fired when the user drags a wire from a source node to a recipe node to add a dependency. */
DECLARE_DELEGATE_TwoParams(FArcOnAddDependency, class UArcRecipeGraphNode* /*Source*/, class UArcRecipeGraphNode* /*TargetRecipe*/);

/**
 * UEdGraph subclass for the ArcCraft recipe dependency graph.
 *
 * The constructor sets Schema = UArcRecipeGraphSchema::StaticClass() so that
 * SGraphEditor never null-derefs the schema pointer on the first layout or
 * context-menu call (mirrors UBehaviorTreeGraph ctor, BehaviorTreeGraph.cpp:46).
 *
 * The three delegates are bound by the owning widget (SArcRecipeGraphView) so
 * that the schema can fire them from context-menu actions without knowing about
 * the widget layer.
 */
UCLASS()
class ARCCRAFTEDITOR_API UArcRecipeGraph : public UEdGraph
{
	GENERATED_BODY()

public:

	UArcRecipeGraph(const FObjectInitializer& ObjectInitializer);

	/** Bound by the owning widget to isolate the subgraph for the given recipe. */
	FArcOnIsolateRecipe OnIsolateRecipe;

	/** Bound by the owning widget to restore the full graph. */
	FArcOnShowAll OnShowAll;

	/** Bound by the owning widget to open the recipe asset in its editor. */
	FArcOnOpenAsset OnOpenAsset;

	/** Bound by the owning widget when a TagGroup member row is single-clicked. */
	FArcOnSelectMember OnSelectMember;

	/** Bound by the owning widget to create a new item definition asset. */
	FArcOnCreateAsset OnCreateItem;

	/** Bound by the owning widget to create a new recipe definition asset. */
	FArcOnCreateAsset OnCreateRecipe;

	/** Bound by the owning widget to add a dependency edge between two recipe nodes. */
	FArcOnAddDependency OnAddDependency;

	/** AssetId of the currently-selected TagGroup member row; drives row highlight. Set by the view. */
	UPROPERTY(Transient)
	FPrimaryAssetId SelectedMemberId;

	/** True while a chain highlight is active (a node is selected); drives wire styling. */
	UPROPERTY(Transient)
	bool bChainHighlightActive = false;

	/** (FromNode, ToNode) of every optional edge; filled by PopulateGraph, read by the connection drawing policy. */
	TSet<TTuple<FPrimaryAssetId, FPrimaryAssetId>> OptionalEdges;
};
