// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Collision/ArcTerrainHeightfieldRef.h"
#include "ArcTerrainCollisionComponent.generated.h"

/**
 * Per-cell heightfield collision (one transient component per collision-resident cell, created on
 * the AArcTerrain by the subsystem). Direct-Chaos, NO UBodySetup — heightfields have no body-setup
 * injection path — following ULandscapeHeightfieldCollisionComponent::OnCreatePhysicsState
 * (LandscapeCollision.cpp): USceneComponent::OnCreatePhysicsState bypass, FPhysicsInterface
 * CreateActor, TImplicitObjectTransformed-wrapped heightfield shape, filter data from the BlockAll
 * profile, AddActorsToScene_AssumesLocked under ExecuteWrite, AddToComponentMaps.
 *
 * Never rendered; exists purely for physics + navmesh (DoCustomNavigableGeometryExport ->
 * ExportChaosHeightField). Mobility stays Movable so attaching to the (movable) terrain root never
 * warns — the Chaos actor is created bStatic regardless (Landscape passes bStatic unconditionally).
 */
UCLASS(NotBlueprintable, ClassGroup = (ArcTerrain), HideCategories = (Rendering, Lighting, LOD, TextureStreaming, Mobility))
class ARCTERRAIN_API UArcTerrainCollisionComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UArcTerrainCollisionComponent();

	/**
	 * Swap in a (new) heightfield and recreate the physics state. Double-buffered by construction:
	 * the OLD geometry stays alive through the destroy/create window — the outgoing physics actor
	 * holds its own Chaos ref (see FArcTerrainHeightfieldRef) — so queries in flight never touch
	 * freed geometry. The sample->local scale travels INSIDE the ref (BaseScale).
	 */
	void SetHeightfield(TRefCountPtr<FArcTerrainHeightfieldRef> InHeightfieldRef);

	//~ Begin UPrimitiveComponent / USceneComponent interface
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End UPrimitiveComponent / USceneComponent interface

private:
	/** Plain ref-counted CPU object — not a UObject, no UPROPERTY. */
	TRefCountPtr<FArcTerrainHeightfieldRef> HeightfieldRef;

	/** Component-local heightfield bounds, cached at SetHeightfield from the geometry's BASE-scale
	 *  bounding box (before physics creation bakes the world scale into the geometry — CalcBounds'
	 *  LocalToWorld applies the world scale itself). */
	FBox LocalBounds;
};
