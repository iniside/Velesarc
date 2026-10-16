// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainStampVisualizer.h"
#include "Stamps/ArcTerrainStampComponent.h"

void FArcTerrainStampVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (const UArcTerrainStampComponent* StampComponent = Cast<UArcTerrainStampComponent>(Component))
	{
		StampComponent->DrawStampVisualization(View, PDI);
	}
}
