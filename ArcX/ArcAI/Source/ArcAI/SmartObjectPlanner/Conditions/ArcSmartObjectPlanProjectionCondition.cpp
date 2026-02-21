// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanProjectionCondition.h"

#include "MassEntityManager.h"
#include "NavigationSystem.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanProjectionCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FMassEntityManager& EntityManager) const
{
	const UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return false;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}

	FNavLocation ProjectedLocation;
	const bool bProjected = NavSys->ProjectPointToNavigation(
		Entity.Location,
		ProjectedLocation,
		ProjectionExtent);

	return bProjected;
}
