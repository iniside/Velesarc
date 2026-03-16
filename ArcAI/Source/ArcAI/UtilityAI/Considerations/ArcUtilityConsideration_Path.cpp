// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_Path.h"
#include "MassEntityManager.h"
#include "NavigationSystem.h"
#include "Engine/World.h"

float FArcUtilityConsideration_PathLength::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	UWorld* World = Context.World.Get();
	if (!World)
	{
		return 0.0f;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return 0.0f;
	}

	const FVector TargetLoc = Target.GetLocation(Context.EntityManager);

	FPathFindingQuery Query;
	Query.StartLocation = Context.QuerierLocation;
	Query.EndLocation = TargetLoc;

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		return 0.0f;
	}
	Query.NavData = NavData;

	FPathFindingResult Result = NavSys->FindPathSync(Query);
	if (!Result.IsSuccessful() || !Result.Path.IsValid())
	{
		return 0.0f;
	}

	const float PathLength = Result.Path->GetLength();
	return FMath::Clamp(1.0f - (PathLength / MaxPathLength), 0.0f, 1.0f);
}

float FArcUtilityConsideration_PathExists::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	UWorld* World = Context.World.Get();
	if (!World)
	{
		return 0.0f;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return 0.0f;
	}

	const FVector TargetLoc = Target.GetLocation(Context.EntityManager);

	FPathFindingQuery Query;
	Query.StartLocation = Context.QuerierLocation;
	Query.EndLocation = TargetLoc;

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		return 0.0f;
	}
	Query.NavData = NavData;

	const bool bPathExists = NavSys->TestPathSync(Query);
	return bPathExists ? 1.0f : 0.0f;
}
