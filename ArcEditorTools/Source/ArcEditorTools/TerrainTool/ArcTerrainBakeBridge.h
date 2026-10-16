// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Editor-side bridge that services UArcTerrainBakedStamp::Bake() requests. Binds the Runtime stamp's
 * static OnBakeRequested delegate to an editor handler that evaluates the stamp's BakeSource (a
 * UArcHeightmapGraph or a UTextureGraph) and fills the stamp's baked fields. Keeps the dependency
 * one-way: ArcEditorTools depends on ArcTerrain, never the reverse.
 */
namespace ArcTerrainBakeBridge
{
	/** Bind the bake handler (call from FArcEditorToolsModule::StartupModule). */
	void Register();

	/** Unbind the bake handler (call from FArcEditorToolsModule::ShutdownModule). */
	void Unregister();
}
