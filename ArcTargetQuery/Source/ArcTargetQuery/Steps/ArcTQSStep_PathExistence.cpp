// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_PathExistence.h"
#include "ArcMass/Helpers/ArcNavHelpers.h"

float FArcTQSStep_PathExistence::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	return ArcNavHelpers::PathExists(QueryContext.World.Get(), ReferenceLocation, Item.GetLocation(QueryContext.EntityManager)) ? 1.0f : 0.0f;
}
