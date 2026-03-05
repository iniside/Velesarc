// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_DistanceFilter.h"

float FArcTQSStep_DistanceFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FVector ItemLocation = Item.GetLocation(QueryContext.EntityManager);

	// Resolve reference locations
	TArray<FVector> RefLocations;
	LocationConfig.ResolveLocations(Item, QueryContext, RefLocations);

	// Pass if item is within range of ANY reference location
	for (const FVector& RefLoc : RefLocations)
	{
		const float Distance = FVector::Dist(RefLoc, ItemLocation);
		if (Distance >= MinDistance && Distance <= MaxDistance)
		{
			return 1.0f;
		}
	}

	return 0.0f;
}
