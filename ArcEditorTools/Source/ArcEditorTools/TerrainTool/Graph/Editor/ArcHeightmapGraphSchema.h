// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphSchema.h"

#include "ArcHeightmapGraphSchema.generated.h"

class UScriptStruct;

/**
 * Schema action that spawns a new heightmap graph node mirroring NodeStruct.
 *
 * PerformAction appends a fresh FInstancedStruct of NodeStruct to the source
 * asset's Nodes list (under a transaction), then creates the mirror
 * UArcHeightmapEdGraphNode keyed by the new NodeId.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapGraphSchemaAction_NewNode : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	/** Concrete FArcHeightmapNode subtype this action instantiates. */
	UPROPERTY()
	TObjectPtr<UScriptStruct> NodeStruct = nullptr;

	FArcHeightmapGraphSchemaAction_NewNode() = default;

	FArcHeightmapGraphSchemaAction_NewNode(FText InNodeCategory, FText InMenuDesc, FText InToolTip,
		UScriptStruct* InNodeStruct)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), 0)
		, NodeStruct(InNodeStruct)
	{}

	using FEdGraphSchemaAction::PerformAction;
	virtual UEdGraphNode* PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin,
		const FVector2f& Location, bool bSelectNewNode = true) override;
};

/**
 * Schema for the heightmap editor graph.
 *
 * - CanCreateConnection: output→input only, no self-loops, no cycles (upstream walk
 *   from the output node), single source per positional input slot (BREAK_OTHERS_B).
 * - TryCreateConnection: makes the engine link, fires OnGraphEdited.
 * - GetGraphContextActions: enumerates concrete FArcHeightmapNode subtypes, with a
 *   single-Output guard.
 *
 * Step 5 adds CreateConnectionDrawingPolicy.
 */
UCLASS()
class ARCEDITORTOOLS_API UArcHeightmapGraphSchema : public UEdGraphSchema
{
	GENERATED_BODY()

public:

	// ---- UEdGraphSchema interface ----------------------------------------------

	virtual const FPinConnectionResponse CanCreateConnection(
		const UEdGraphPin* A,
		const UEdGraphPin* B) const override;

	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;

	virtual void GetGraphContextActions(
		struct FGraphContextMenuBuilder& ContextMenuBuilder) const override;

	virtual void GetContextMenuActions(
		class UToolMenu* Menu,
		class UGraphNodeContextMenuContext* Context) const override;

	virtual class FConnectionDrawingPolicy* CreateConnectionDrawingPolicy(
		int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor,
		const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements,
		class UEdGraph* InGraphObj) const override;
};
