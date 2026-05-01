// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Debug/DebugDrawComponent.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "ArcPlacedEntityCollisionDrawComponent.generated.h"

struct FArcPlacedEntityEntry;

struct FArcPlacedEntityCollisionData
{
	FKAggregateGeom AggGeom;
	TArray<FTransform> InstanceTransforms;
};

UCLASS(ClassGroup = Debug)
class ARCMASS_API UArcPlacedEntityCollisionDrawComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UArcPlacedEntityCollisionDrawComponent();

	void Populate(TConstArrayView<FArcPlacedEntityEntry> Entries);

protected:
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if UE_ENABLE_DEBUG_DRAWING
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
#endif

private:
	TArray<FArcPlacedEntityCollisionData> CachedCollisionData;
	FBox CachedBounds = FBox(ForceInit);
};

#endif // WITH_EDITOR
