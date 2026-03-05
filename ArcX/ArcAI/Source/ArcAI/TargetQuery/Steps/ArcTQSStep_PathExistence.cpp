// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_PathExistence.h"
#include "NavigationSystem.h"

float FArcTQSStep_PathExistence::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryContext.World.Get());
	if (!NavSys)
	{
		return 0.0f;
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		return 0.0f;
	}

	// Resolve start locations
	TArray<FVector> StartLocations;
	LocationConfig.ResolveLocations(Item, QueryContext, StartLocations);

	const FVector EndLocation = Item.GetLocation(QueryContext.EntityManager);

	// Pass if path exists from ANY start location
	for (const FVector& StartLoc : StartLocations)
	{
		FPathFindingQuery Query(nullptr, *NavData, StartLoc, EndLocation);
		Query.bAllowPartialPaths = false;

		if (NavSys->TestPathSync(Query))
		{
			return 1.0f;
		}
	}

	return 0.0f;
}
