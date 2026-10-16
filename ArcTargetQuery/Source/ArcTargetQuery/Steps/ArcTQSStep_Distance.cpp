// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Distance.h"

float FArcTQSStep_Distance::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const float Distance = FVector::Dist(ReferenceLocation, Item.GetLocation(QueryContext.EntityManager));
	return FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);
}
