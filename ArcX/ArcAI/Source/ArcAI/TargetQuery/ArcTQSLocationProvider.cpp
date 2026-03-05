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

void FArcTQSLocationConfig::ResolveLocations(
	const FArcTQSTargetItem& Item,
	const FArcTQSQueryContext& QueryContext,
	TArray<FVector>& OutLocations) const
{
	OutLocations.Reset();

	if (bIncludeQuerierLocation)
	{
		OutLocations.Add(QueryContext.QuerierLocation);
	}

	switch (AdditionalSource)
	{
	case EArcTQSLocationSource::None:
		break;

	case EArcTQSLocationSource::ReferenceLocation:
		OutLocations.Add(ReferenceLocation);
		break;

	case EArcTQSLocationSource::ContextLocations:
		OutLocations.Append(QueryContext.ContextLocations);
		break;

	case EArcTQSLocationSource::CustomProvider:
		if (const FArcTQSLocationProvider* Provider = LocationProvider.GetPtr<FArcTQSLocationProvider>())
		{
			// Use temp array because GetLocations() resets its output
			TArray<FVector> ProviderLocations;
			Provider->GetLocations(Item, QueryContext, ProviderLocations);
			OutLocations.Append(ProviderLocations);
		}
		break;
	}

	// Absolute fallback: always have at least one location
	if (OutLocations.IsEmpty())
	{
		OutLocations.Add(QueryContext.QuerierLocation);
	}
}
