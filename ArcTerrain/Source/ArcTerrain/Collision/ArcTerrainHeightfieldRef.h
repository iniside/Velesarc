// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "Chaos/HeightField.h"

/**
 * Ref-counted holder of one cell's Chaos heightfield (mirrors Landscape's FHeightfieldGeometryRef;
 * FRefCountedObject is thread-safe on this branch, so background build tasks and the game thread
 * can share refs). The Chaos geometry
 * itself is also ref-counted (FHeightFieldPtr): a physics actor keeps the heightfield alive through
 * its own FImplicitObjectPtr even after this holder is swapped, so a heightfield replacement never
 * pulls geometry out from under a still-live physics state.
 */
class FArcTerrainHeightfieldRef : public FRefCountedObject
{
public:
	Chaos::FHeightFieldPtr Heightfield;

	/**
	 * (TexelToLocal, TexelToLocal, 1) — heightfield sample index -> terrain-local cm. The heightfield
	 * is CONSTRUCTED with this scale; UArcTerrainCollisionComponent::OnCreatePhysicsState overwrites
	 * the geometry scale with BaseScale * component-world-scale (Chaos actors are rigid, Landscape
	 * pattern), and the navmesh export carries it via the export transform instead (the exporter
	 * reads RAW sample coordinates, ignoring the geometry's internal scale).
	 */
	FVector BaseScale = FVector::OneVector;
};
