// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanPathfindingCondition.h"

#include "MassEntityManager.h"
#include "NavigationSystem.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanPathfindingCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context) const
{
	const UWorld* World = Context.EntityManager->GetWorld();
	if (!World)
	{
		return false;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return false;
	}

	const FVector Start = Context.RequestingLocation;
	const FVector End = Entity.Location;

	FPathFindingQuery Query(nullptr, *NavSys->GetDefaultNavDataInstance(), Start, End);
	Query.NavAgentProperties = NavAgentProperties;

	const FPathFindingResult Result = const_cast<UNavigationSystemV1*>(NavSys)->FindPathSync(Query);

	return Result.IsSuccessful();
}
