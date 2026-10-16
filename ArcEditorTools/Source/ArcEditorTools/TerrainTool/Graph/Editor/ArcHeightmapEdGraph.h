// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EdGraph/EdGraph.h"
#include "UObject/WeakObjectPtr.h"

#include "ArcHeightmapEdGraph.generated.h"

class UArcHeightmapGraph;

/**
 * UEdGraph subclass mirroring a UArcHeightmapGraph asset in the editor.
 *
 * The ctor sets Schema = UArcHeightmapGraphSchema::StaticClass() so SGraphEditor
 * never null-derefs the schema pointer (mirrors UArcRecipeGraph / UBehaviorTreeGraph).
 *
 * SourceAsset is the mirrored data asset; mirror nodes resolve their backing
 * FInstancedStruct against SourceAsset->Nodes via NodeId. NotifyGraphChanged()
 * fans out to OnGraphEdited so native delete/cut/paste/drag operations reach the
 * owning widget's write-back path.
 */
UCLASS()
class ARCEDITORTOOLS_API UArcHeightmapEdGraph : public UEdGraph
{
	GENERATED_BODY()

public:

	UArcHeightmapEdGraph(const FObjectInitializer& ObjectInitializer);

	/** The source asset this editor graph mirrors. */
	UPROPERTY()
	TWeakObjectPtr<UArcHeightmapGraph> SourceAsset;

	/** Bound by the owning widget to trigger write-back when the graph is edited. */
	FSimpleMulticastDelegate OnGraphEdited;

	/** Forwards native graph-change notifications (delete/cut/paste/drag) to OnGraphEdited. */
	virtual void NotifyGraphChanged() override;
};
