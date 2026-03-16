// Copyright Lukasz Baran. All Rights Reserved.

#include "Navigation/ArcInfluenceQueryFilter.h"

#if WITH_RECAST
#include "NavMesh/RecastHelpers.h"
#endif

#include "NavigationData.h"
#include "ArcMass/ArcMassInfluenceMapping.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcInfluenceQueryFilter)

// ---------------------------------------------------------------------------
// FArcInfluenceRecastQueryFilter
// ---------------------------------------------------------------------------

#if WITH_RECAST

FArcInfluenceRecastQueryFilter::FArcInfluenceRecastQueryFilter()
	: FRecastQueryFilter(/*bIsVirtual=*/ true)
{
}

dtReal FArcInfluenceRecastQueryFilter::getVirtualCost(
	const dtReal* pa, const dtReal* pb,
	const dtPolyRef prevRef, const dtMeshTile* prevTile, const dtPoly* prevPoly,
	const dtPolyRef curRef, const dtMeshTile* curTile, const dtPoly* curPoly,
	const dtPolyRef nextRef, const dtMeshTile* nextTile, const dtPoly* nextPoly) const
{
	const dtReal BaseCost = getInlineCost(
		pa, pb,
		prevRef, prevTile, prevPoly,
		curRef, curTile, curPoly,
		nextRef, nextTile, nextPoly);

	if (!InfluenceSubsystem || CostMultiplier == 0.f)
	{
		return BaseCost;
	}

	// pa/pb are in Recast coordinate space — convert to Unreal world space.
	// Sample at the midpoint of the edge for a representative value.
	const dtReal Mid[3] = {
		(pa[0] + pb[0]) * dtReal(0.5),
		(pa[1] + pb[1]) * dtReal(0.5),
		(pa[2] + pb[2]) * dtReal(0.5)
	};
	const FVector WorldPos = Recast2UnrealPoint(Mid);

	const float Influence = InfluenceSubsystem->QueryInfluence(GridIndex, WorldPos, Channel);
	if (Influence <= 0.f)
	{
		return BaseCost;
	}

	// Scale: BaseCost * (1 + Influence * Multiplier)
	// Influence is typically 0..1, Multiplier controls sensitivity.
	return BaseCost * dtReal(1.0 + double(Influence) * double(CostMultiplier));
}

INavigationQueryFilterInterface* FArcInfluenceRecastQueryFilter::CreateCopy() const
{
	return new FArcInfluenceRecastQueryFilter(*this);
}

#endif // WITH_RECAST

// ---------------------------------------------------------------------------
// UArcInfluenceNavigationQueryFilter
// ---------------------------------------------------------------------------

UArcInfluenceNavigationQueryFilter::UArcInfluenceNavigationQueryFilter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bInstantiateForQuerier = true;
}

void UArcInfluenceNavigationQueryFilter::InitializeFilter(
	const ANavigationData& NavData,
	const UObject* Querier,
	FNavigationQueryFilter& Filter) const
{
#if WITH_RECAST
	const UWorld* World = NavData.GetWorld();
	if (World)
	{
		FArcInfluenceRecastQueryFilter CustomFilter;
		CustomFilter.InfluenceSubsystem = World->GetSubsystem<UArcInfluenceMappingSubsystem>();
		CustomFilter.GridIndex = GridIndex;
		CustomFilter.Channel = Channel;
		CustomFilter.CostMultiplier = CostMultiplier;

		// SetFilterImplementation calls CreateCopy() internally, so the
		// stack-local filter is safe — it gets deep-copied into the shared ptr.
		Filter.SetFilterImplementation(&CustomFilter);
	}
#endif

	// Apply any area cost/flag overrides configured in the UObject properties.
	Super::InitializeFilter(NavData, Querier, Filter);
}
