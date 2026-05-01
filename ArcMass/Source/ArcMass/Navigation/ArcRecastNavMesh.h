// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavMesh/RecastNavMesh.h"
#include "ArcRecastNavMesh.generated.h"

class UArcMassNavInvokerSubsystem;

/**
 * Recast navmesh subclass that protects Mass-owned tiles from the engine's
 * active-tile eviction cycle. Tiles tracked by UArcMassNavInvokerSubsystem
 * are filtered from RemoveTiles and re-inserted into ActiveTileSet after
 * the base UpdateActiveTiles pass.
 *
 * RebuildTile is non-virtual in ARecastNavMesh, so it cannot be filtered here.
 * The Mass pipeline's rebuild calls go directly to the base implementation,
 * which is the desired behavior (no filtering needed for Mass-initiated rebuilds).
 */
UCLASS()
class ARCMASS_API AArcRecastNavMesh : public ARecastNavMesh
{
	GENERATED_BODY()

public:
	virtual void UpdateActiveTiles(const TArray<FNavigationInvokerRaw>& InvokerLocations) override;
	virtual void RemoveTiles(const TArray<FIntPoint>& Tiles) override;

private:
	friend UArcMassNavInvokerSubsystem;

	/** RAII guard that sets bInsideMassApply for the lifetime of the scope. */
	struct FMassApplyScope
	{
		AArcRecastNavMesh* NavMesh;
		explicit FMassApplyScope(AArcRecastNavMesh* InNavMesh) : NavMesh(InNavMesh)
		{
			if (NavMesh != nullptr) { NavMesh->bInsideMassApply = true; }
		}
		~FMassApplyScope()
		{
			if (NavMesh != nullptr) { NavMesh->bInsideMassApply = false; }
		}
		FMassApplyScope(const FMassApplyScope&) = delete;
		FMassApplyScope& operator=(const FMassApplyScope&) = delete;
	};

	/** When true, RemoveTiles skips the Mass-tile filter. Set by the Mass pipeline before its own remove calls. */
	bool bInsideMassApply = false;
};
