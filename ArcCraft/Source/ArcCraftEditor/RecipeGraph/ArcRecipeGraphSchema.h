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
#include "EdGraph/EdGraphSchema.h"

#include "ArcRecipeGraphSchema.generated.h"

/**
 * Schema action that creates a new Item or Recipe definition asset.
 * Triggered from the graph background context menu (no FromPin).
 * PerformAction fires the appropriate delegate on UArcRecipeGraph; the owning
 * widget is responsible for the actual asset creation.
 */
USTRUCT()
struct ARCCRAFTEDITOR_API FArcRecipeGraphSchemaAction_Create : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	/** True → create a recipe; false → create an item. */
	UPROPERTY()
	bool bIsRecipe = false;

	FArcRecipeGraphSchemaAction_Create() = default;

	FArcRecipeGraphSchemaAction_Create(FText InMenuDesc, FText InToolTip, bool bInIsRecipe)
		: FEdGraphSchemaAction(FText::GetEmpty(), MoveTemp(InMenuDesc), MoveTemp(InToolTip), 0)
		, bIsRecipe(bInIsRecipe)
	{}

	using FEdGraphSchemaAction::PerformAction;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin,
		const FVector2f& Location, bool bSelectNewNode = true) override;
};

/**
 * Schema for the ArcCraft recipe dependency graph.
 *
 * Responsibilities:
 *  - Allow user-initiated connections (output → input on Recipe nodes only),
 *    with cycle detection; fires UArcRecipeGraph::OnAddDependency.
 *  - Provide the node context menu for Recipe nodes: "Isolate Dependencies",
 *    "Show All", "Open Recipe Asset" — fired via UArcRecipeGraph delegates so
 *    the schema never couples to the widget layer.
 *  - Provide background context actions: "Create Item", "Create Recipe".
 *
 * Modelled on USoundCueGraphSchema (AudioEditor) for exact 5.7 override signatures.
 */
UCLASS()
class ARCCRAFTEDITOR_API UArcRecipeGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:

	// ---- UEdGraphSchema interface ----------------------------------------------

	/**
	 * Validates a proposed pin connection.
	 * Allows output→input connections where the target node is a Recipe node,
	 * rejects same-direction, self-loops, non-recipe targets, and cycles.
	 */
	virtual const FPinConnectionResponse CanCreateConnection(
		const UEdGraphPin* A,
		const UEdGraphPin* B) const override;

	/**
	 * Commits an authoring connection: fires UArcRecipeGraph::OnAddDependency
	 * and returns false so no engine pin link is created (the view rebuilds).
	 */
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;

	/**
	 * Adds context-menu entries for Recipe nodes:
	 *  - "Isolate Dependencies" → fires UArcRecipeGraph::OnIsolateRecipe
	 *  - "Show All"             → fires UArcRecipeGraph::OnShowAll
	 *  - "Open Recipe Asset"   → fires UArcRecipeGraph::OnOpenAsset
	 *
	 * The schema casts Context->Graph to UArcRecipeGraph* and Context->Node to
	 * UArcRecipeGraphNode*; both casts guard against null before firing.
	 */
	virtual void GetContextMenuActions(
		class UToolMenu* Menu,
		class UGraphNodeContextMenuContext* Context) const override;

	/**
	 * Adds background context-menu entries when no FromPin is active:
	 *  - "Create Item"   → fires UArcRecipeGraph::OnCreateItem
	 *  - "Create Recipe" → fires UArcRecipeGraph::OnCreateRecipe
	 */
	virtual void GetGraphContextActions(
		struct FGraphContextMenuBuilder& ContextMenuBuilder) const override;

	virtual class FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(
		int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
		const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements,
		class UEdGraph* InGraphObj) const override;
};
