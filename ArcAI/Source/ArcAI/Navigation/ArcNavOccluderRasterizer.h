// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "ArcNavOccluderRasterizer.generated.h"

/**
 * Drains the occluder-dirty queue of every tiled influence grid on the game thread and rebuilds
 * each popped tile's occluder bitplane from navmesh boundary edges.
 *
 * ArcMass owns the queue but cannot reach navmesh code, so this consumer lives in ArcAI.
 */
UCLASS()
class ARCAI_API UArcNavOccluderRasterizerProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcNavOccluderRasterizerProcessor();

protected:
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	/**
	 * Per-grid private drain list, refilled from the grid's dirty queue once it runs dry. ArcMass removes
	 * from that queue by swap, so a positional cursor into it would step over undrained tiles.
	 */
	TArray<TArray<FIntPoint>> PendingDrains;

	/** Lifetime tile-update total last seen on the navmesh; any change re-dirties every live tile. */
	uint64 LastNavMeshUpdateCount = 0;

	bool bLoggedPolyGatherTruncation = false;
};
