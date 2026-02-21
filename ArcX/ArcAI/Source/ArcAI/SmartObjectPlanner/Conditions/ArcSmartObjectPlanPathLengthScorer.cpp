// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanPathLengthScorer.h"

#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

float FArcSmartObjectPlanPathLengthScorer::ScoreEntity(
	const FArcPotentialEntity& Entity,
	const FMassEntityManager& EntityManager) const
{
	const UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return 0.f;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return 0.f;
	}

	const FTransformFragment* RequestingTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity.RequestingEntity);
	if (!RequestingTransform)
	{
		return 0.f;
	}

	const FVector Start = RequestingTransform->GetTransform().GetLocation();
	const FVector End = Entity.Location;

	FPathFindingQuery Query(nullptr, *NavSys->GetDefaultNavDataInstance(), Start, End);
	Query.NavAgentProperties = NavAgentProperties;

	const FPathFindingResult Result = const_cast<UNavigationSystemV1*>(NavSys)->FindPathSync(Query);

	if (!Result.IsSuccessful() || !Result.Path.IsValid())
	{
		return 0.f;
	}

	const float PathLength = Result.Path->GetLength();

	if (MaxPathLength <= 0.f || PathLength >= MaxPathLength)
	{
		return 0.f;
	}

	// Shorter paths get higher scores (1.0 = at origin, 0.0 = at max distance)
	return 1.f - (PathLength / MaxPathLength);
}
