// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_PathLength.h"
#include "TargetQuery/ArcTQSLocationProvider.h"
#include "NavigationSystem.h"
#include "NavigationData.h"

float FArcTQSStep_PathLength::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
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

	FPathFindingResult Result = NavSys->FindPathSync(Query);
	if (!Result.IsSuccessful() || !Result.Path.IsValid())
	{
		return 0.0f;
	}

	const float PathLength = static_cast<float>(Result.Path->GetLength());
	const float NormalizedLength = FMath::Clamp(PathLength / MaxPathLength, 0.0f, 1.0f);

	return ResponseCurve.Evaluate(NormalizedLength);
}
