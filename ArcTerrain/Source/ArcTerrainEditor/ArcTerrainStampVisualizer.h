// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ComponentVisualizer.h"

/**
 * Component visualizer for UArcTerrainStampComponent (and subclasses): forwards drawing to the
 * component's own DrawStampVisualization (Stamps/ArcTerrainStampComponent.h) so each stamp type can
 * extend the base wire-box + circle footprint with its own falloff-mask detail. Registered by
 * FArcTerrainEditorModule::StartupModule (OnPostEngineInit).
 */
class FArcTerrainStampVisualizer : public FComponentVisualizer
{
public:
	//~ Begin FComponentVisualizer interface
	virtual void DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI) override;
	//~ End FComponentVisualizer interface
};
