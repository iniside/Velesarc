// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DebugDraw.h"

class AArcTerrain;
struct FArcTerrainState;

/**
 * Per-terrain spatial overlay driven by the AArcTerrain::bDraw* authored knobs: what the counters count,
 * drawn where it is.
 *
 * GAME THREAD, once per subsystem tick, and only for terrains with AArcTerrain::HasDebugOverlayEnabled().
 * UE::DebugDraw primitives live exactly ONE frame, so every enabled layer is re-submitted every tick —
 * there is no persistent set to clear. The manifest-coverage layer BUILDS its lines once and re-submits
 * that array: it answers what is baked, which no camera move changes.
 */
namespace ArcTerrainDebugDraw
{
	ARCTERRAIN_API void DrawTerrain(const AArcTerrain& Terrain, const FArcTerrainState& State,
		TConstArrayView<FVector> InvokerPositions);

	/** Drops Terrain's built manifest-coverage geometry, if any. Call from the terrain's unregister path —
	 *  otherwise nothing ever rebuilds to prune it, and it survives to process exit. Cheap even when the
	 *  layer was never enabled: a miss on an empty map. No-op when UE_DEBUG_RENDERING_ENABLED == 0. */
	ARCTERRAIN_API void EvictCoverageCache(const AArcTerrain& Terrain);
}
