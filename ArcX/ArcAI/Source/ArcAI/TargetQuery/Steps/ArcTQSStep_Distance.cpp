// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Distance.h"

float FArcTQSStep_Distance::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FVector ItemLocation = Item.GetLocation(QueryContext.EntityManager);
	
	const float Distance = FVector::Dist(QueryContext.QuerierLocation, ItemLocation);
	const float NormalizedDistance = FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);
	const float Score = ResponseCurve.Evaluate(NormalizedDistance);
	
	return Score;
}
