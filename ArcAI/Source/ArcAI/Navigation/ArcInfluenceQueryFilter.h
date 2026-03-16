// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"

#if WITH_RECAST
#include "NavMesh/RecastQueryFilter.h"
#endif

#include "ArcInfluenceQueryFilter.generated.h"

class UArcInfluenceMappingSubsystem;

// ---------------------------------------------------------------------------
// FArcInfluenceRecastQueryFilter
//
// Recast-level query filter that samples the ArcMass influence mapping
// subsystem during A* and modifies traversal cost per polygon edge.
//
// The filter is lightweight — it holds a raw pointer to the influence
// subsystem (whose lifetime is the world) and does a simple grid lookup
// in getVirtualCost(). No allocations, no complex queries.
// ---------------------------------------------------------------------------

#if WITH_RECAST

class ARCAI_API FArcInfluenceRecastQueryFilter : public FRecastQueryFilter
{
public:
	FArcInfluenceRecastQueryFilter();

	// -- Configuration (set before pathfinding) ------------------------------

	/** The influence map subsystem to sample. Must outlive the query. */
	const UArcInfluenceMappingSubsystem* InfluenceSubsystem = nullptr;

	/** Which influence grid index to sample (matches project settings order). */
	int32 GridIndex = 0;

	/** Which channel within the grid to sample. */
	int32 Channel = 0;

	/** Multiplier applied to the sampled influence value before adding to cost.
	 *  Final cost = BaseCost * (1 + SampledInfluence * CostMultiplier).
	 *  Higher values make agents avoid/prefer influenced areas more aggressively. */
	float CostMultiplier = 1.f;

	// -- dtQueryFilter overrides ---------------------------------------------

	virtual dtReal getVirtualCost(
		const dtReal* pa, const dtReal* pb,
		const dtPolyRef prevRef, const dtMeshTile* prevTile, const dtPoly* prevPoly,
		const dtPolyRef curRef, const dtMeshTile* curTile, const dtPoly* curPoly,
		const dtPolyRef nextRef, const dtMeshTile* nextTile, const dtPoly* nextPoly) const override;

	// -- INavigationQueryFilterInterface -------------------------------------

	virtual INavigationQueryFilterInterface* CreateCopy() const override;
};

#endif // WITH_RECAST

// ---------------------------------------------------------------------------
// UArcInfluenceNavigationQueryFilter
//
// UObject wrapper that plugs FArcInfluenceRecastQueryFilter into the
// standard UE navigation query pipeline. Assign this filter class on
// AI controllers or pass it to pathfinding requests.
//
// bInstantiateForQuerier is true — each path request gets a fresh filter
// configured with the current influence state. No caching, no stale data.
// ---------------------------------------------------------------------------

UCLASS(Blueprintable, meta = (DisplayName = "Arc Influence Nav Filter"))
class ARCAI_API UArcInfluenceNavigationQueryFilter : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	UArcInfluenceNavigationQueryFilter(const FObjectInitializer& ObjectInitializer);

	/** Which influence grid to sample (index into project settings grid array). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence")
	int32 GridIndex = 0;

	/** Which channel within the grid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence")
	int32 Channel = 0;

	/** Cost multiplier. Final cost = BaseCost * (1 + Influence * Multiplier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Influence", meta = (ClampMin = "0.0"))
	float CostMultiplier = 1.f;

protected:
	virtual void InitializeFilter(const ANavigationData& NavData, const UObject* Querier, FNavigationQueryFilter& Filter) const override;
};
