// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraphNode.h"

#include "ArcHeightmapEdGraphNode.generated.h"

class UEdGraphPin;

/** Shared pin category for heightmap graph connections (Steps 3/5 reference this). */
namespace ArcHeightmapPins
{
	extern ARCEDITORTOOLS_API const FName HeightField;
}

/**
 * UEdGraphNode subclass mirroring one FArcHeightmapNode of the source asset.
 *
 * The node carries no copy of the data — NodeId keys back into the owning
 * UArcHeightmapEdGraph::SourceAsset->Nodes list, and pins/title/color are all
 * derived from the resolved FInstancedStruct so the mirror stays a thin view.
 */
UCLASS()
class ARCEDITORTOOLS_API UArcHeightmapEdGraphNode : public UEdGraphNode
{
	GENERATED_BODY()

public:

	/** Identity into the source asset's Nodes list. */
	UPROPERTY()
	FGuid NodeId;

	// ---- UEdGraphNode interface -------------------------------------------------

	/** Creates GetInputCount() input pins + one output pin from the mirrored node. */
	virtual void AllocateDefaultPins() override;

	/** Display name from the mirrored struct (ArcHeightmapNode_ prefix stripped). */
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;

	/** Colour keyed by mirrored struct type (Noise/Erosion/Math/Constant/Output). */
	virtual FLinearColor GetNodeTitleColor() const override;

	/** All nodes deletable except the FArcHeightmapNode_Output result marker. */
	virtual bool CanUserDeleteNode() const override;

	// ---- Accessors --------------------------------------------------------------

	/** Returns the Index-th input pin, or nullptr if out of range. */
	UEdGraphPin* GetInputPin(int32 Index) const;

	/** Returns the single output pin, or nullptr. */
	UEdGraphPin* GetOutputPin() const;
};
