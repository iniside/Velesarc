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
#include "EdGraph/EdGraphNode.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "RecipeGraph/ArcRecipeGraphData.h"

#include "ArcRecipeGraphNode.generated.h"

class UEdGraphPin;

/** Transient per-node highlight state driven by chain-selection in the view. */
enum class EArcRecipeNodeHighlight : uint8
{
	Normal,
	Dimmed,
};

/**
 * UEdGraphNode subclass representing one node in the ArcCraft recipe dependency graph.
 *
 * A single class covers all three node kinds (Recipe, RawItem, TagGroup);
 * NodeType discriminates behaviour in AllocateDefaultPins, GetNodeTitleColor, etc.
 *
 * Context-menu actions are owned entirely by UArcRecipeGraphSchema —
 * GetNodeContextMenuActions is intentionally NOT overridden here.
 */
UCLASS()
class ARCCRAFTEDITOR_API UArcRecipeGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:

	/** Discriminates Recipe / RawItem / TagGroup behaviour. */
	UPROPERTY()
	EArcRecipeGraphNodeType NodeType = EArcRecipeGraphNodeType::Recipe;

	/** Identity key: recipe asset id, raw item asset id, or synthesised tag-group key. */
	UPROPERTY()
	FPrimaryAssetId AssetId;

	/** For TagGroup nodes: the RequiredTags that define the group. */
	UPROPERTY()
	FGameplayTagContainer GroupTags;

	/** For TagGroup nodes: the raw item asset ids that belong to this group. */
	UPROPERTY()
	TArray<FPrimaryAssetId> GroupMembers;

	/** For TagGroup nodes: display titles parallel to GroupMembers. */
	UPROPERTY()
	TArray<FText> GroupMemberTitles;

	/** Cached display label set by PopulateGraph from FArcRecipeGraphNodeData::Title. */
	UPROPERTY()
	FText DisplayTitle;

	/** View-driven highlight state; read live by SArcRecipeGraphNode render tint. Not reflected. */
	EArcRecipeNodeHighlight HighlightState = EArcRecipeNodeHighlight::Normal;

	// ---- UEdGraphNode interface -------------------------------------------------

	/** Read-only view: nodes may be dragged but never deleted or duplicated. */
	virtual bool CanUserDeleteNode() const override { return false; }
	virtual bool CanDuplicateNode() const override { return false; }

	/** Creates input and/or output pins according to NodeType. */
	virtual void AllocateDefaultPins() override;

	/** Returns DisplayTitle for all title-type variants. */
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	/** For TagGroup: lists GroupTags and member count; for others: the AssetId string. */
	virtual FText GetTooltipText() const override;

	/**
	 * Returns a colour keyed by NodeType:
	 *   Recipe   — teal   FColor(72, 191, 145)
	 *   RawItem  — orange FColor(230, 145,  60)
	 *   TagGroup — indigo FColor(100, 120, 220)
	 * Takes effect once the Step-4 visual node factory overrides the default SGraphNode.
	 */
	virtual FLinearColor GetNodeTitleColor() const override;

	// ---- Accessors --------------------------------------------------------------

	/** Returns the input pin, or nullptr if this node type has none. */
	UEdGraphPin* GetInputPin() const;

	/** Returns the output pin (all node types have one). */
	UEdGraphPin* GetOutputPin() const;
};
