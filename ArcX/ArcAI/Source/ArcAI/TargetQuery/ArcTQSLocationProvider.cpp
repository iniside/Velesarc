// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSLocationProvider.h"

FVector FArcTQSLocationProvider_NearestContext::GetLocation(
	const FArcTQSTargetItem& Item,
	const FArcTQSQueryContext& QueryContext) const
{
	if (QueryContext.ContextLocations.IsEmpty())
	{
		return QueryContext.QuerierLocation;
	}

	// Fast path: generator stamped the context index on this item
	if (QueryContext.ContextLocations.IsValidIndex(Item.ContextIndex))
	{
		return QueryContext.ContextLocations[Item.ContextIndex];
	}

	// Fallback for items without a stamped context (e.g. explicit lists):
	// find nearest context by distance
	const FVector ItemLocation = Item.Location;
	float BestDistSq = MAX_FLT;
	FVector BestLocation = QueryContext.ContextLocations[0];

	for (const FVector& CtxLoc : QueryContext.ContextLocations)
	{
		const float DistSq = FVector::DistSquared(ItemLocation, CtxLoc);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			BestLocation = CtxLoc;
		}
	}

	return BestLocation;
}

FVector FArcTQSLocationProvider_ContextByIndex::GetLocation(
	const FArcTQSTargetItem& Item,
	const FArcTQSQueryContext& QueryContext) const
{
	if (QueryContext.ContextLocations.IsValidIndex(ContextIndex))
	{
		return QueryContext.ContextLocations[ContextIndex];
	}

	return QueryContext.QuerierLocation;
}
