// Copyright Lukasz Baran. All Rights Reserved.

#include "TerrainTool/Graph/Editor/ArcHeightmapEdGraph.h"

#include "TerrainTool/Graph/Editor/ArcHeightmapGraphSchema.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcHeightmapEdGraph)

UArcHeightmapEdGraph::UArcHeightmapEdGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Schema = UArcHeightmapGraphSchema::StaticClass();
}

void UArcHeightmapEdGraph::NotifyGraphChanged()
{
	Super::NotifyGraphChanged();
	OnGraphEdited.Broadcast();
}
