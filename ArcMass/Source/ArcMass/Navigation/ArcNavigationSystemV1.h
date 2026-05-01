// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavigationSystem.h"
#include "ArcNavigationSystemV1.generated.h"

/**
 * Navigation system subclass that protects Mass-owned navmesh tiles from being
 * evicted during the World Partition dynamic navmesh active-tile update pass.
 *
 * On each UpdateNavDataActiveTiles call the engine recomputes which tiles are
 * "active" based on registered invokers.  Because Mass invokers are managed
 * entirely through UArcMassNavInvokerSubsystem (not through the engine invoker
 * list) their tiles would otherwise be cleared.  This override re-inserts every
 * tile owned by the subsystem into each ARecastNavMesh's active-tile set after
 * the base implementation has run, ensuring they are never evicted.
 *
 * For non-World-Partition levels (static navmesh) this is a harmless no-op.
 */
UCLASS()
class ARCMASS_API UArcNavigationSystemV1 : public UNavigationSystemV1
{
	GENERATED_BODY()

protected:
	virtual void UpdateNavDataActiveTiles() override;
};
