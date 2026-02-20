// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_DistanceFilter.h"

float FArcTQSStep_DistanceFilter::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FVector ItemLocation = Item.GetLocation(QueryContext.EntityManager);
	const float Distance = FVector::Dist(QueryContext.QuerierLocation, ItemLocation);

	return (Distance >= MinDistance && Distance <= MaxDistance) ? 1.0f : 0.0f;
}
