// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_NavProjection.h"
#include "NavigationSystem.h"

float FArcTQSStep_NavProjection::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryContext.World.Get());
	if (!NavSys)
	{
		return 0.0f;
	}

	const FVector ItemLocation = Item.GetLocation(QueryContext.EntityManager);
	const FVector QueryExtent(HorizontalExtent, HorizontalExtent, ProjectionExtent);

	FNavLocation ProjectedLocation;
	if (NavSys->ProjectPointToNavigation(ItemLocation, ProjectedLocation, QueryExtent))
	{
		// Update item location to projected point
		const_cast<FArcTQSTargetItem&>(Item).Location = ProjectedLocation.Location;
		return 1.0f;
	}

	return 0.0f;
}
