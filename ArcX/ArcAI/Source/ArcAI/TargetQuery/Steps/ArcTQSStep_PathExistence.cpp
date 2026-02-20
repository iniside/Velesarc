// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_PathExistence.h"
#include "TargetQuery/ArcTQSLocationProvider.h"
#include "NavigationSystem.h"

float FArcTQSStep_PathExistence::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryContext.World.Get());
	if (!NavSys)
	{
		return 0.0f;
	}

	// Resolve start location
	FVector StartLocation;
	if (bUseLocationProvider)
	{
		if (const FArcTQSLocationProvider* Provider = LocationProvider.GetPtr<FArcTQSLocationProvider>())
		{
			StartLocation = Provider->GetLocation(Item, QueryContext);
		}
		else
		{
			StartLocation = QueryContext.ContextLocations.IsValidIndex(Item.ContextIndex)
				? QueryContext.ContextLocations[Item.ContextIndex]
				: QueryContext.QuerierLocation;
		}
	}
	else
	{
		StartLocation = PathStart;
	}

	const FVector EndLocation = Item.GetLocation(QueryContext.EntityManager);

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		return 0.0f;
	}

	FPathFindingQuery Query(nullptr, *NavData, StartLocation, EndLocation);
	Query.bAllowPartialPaths = false;

	const bool bPathExists = NavSys->TestPathSync(Query);
	return bPathExists ? 1.0f : 0.0f;
}
