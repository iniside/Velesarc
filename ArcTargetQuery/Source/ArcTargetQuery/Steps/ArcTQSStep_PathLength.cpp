// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_PathLength.h"
#include "ArcMass/Helpers/ArcNavHelpers.h"

float FArcTQSStep_PathLength::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const TOptional<float> Len = ArcNavHelpers::PathLength(QueryContext.World.Get(), ReferenceLocation, Item.GetLocation(QueryContext.EntityManager));
	if (!Len.IsSet())
	{
		return UnscorableRaw;
	}
	return FMath::Clamp(Len.GetValue() / MaxPathLength, 0.0f, 1.0f);
}
