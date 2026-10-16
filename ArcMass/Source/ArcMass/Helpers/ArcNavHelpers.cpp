// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcNavHelpers.h"

#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"

bool ArcNavHelpers::PathExists(
	const UWorld* World,
	const FVector& From,
	const FVector& To,
	const FNavAgentProperties& NavAgentProps)
{
	if (!World)
	{
		return false;
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(const_cast<UWorld*>(World));
	if (!NavSys)
	{
		return false;
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		return false;
	}

	FPathFindingQuery Query(nullptr, *NavData, From, To);
	Query.bAllowPartialPaths = false;
	Query.NavAgentProperties = NavAgentProps;

	return NavSys->TestPathSync(Query);
}

TOptional<float> ArcNavHelpers::PathLength(
	const UWorld* World,
	const FVector& From,
	const FVector& To,
	const FNavAgentProperties& NavAgentProps)
{
	if (!World)
	{
		return {};
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(const_cast<UWorld*>(World));
	if (!NavSys)
	{
		return {};
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		return {};
	}

	FPathFindingQuery Query(nullptr, *NavData, From, To);
	Query.bAllowPartialPaths = false;
	Query.NavAgentProperties = NavAgentProps;

	const FPathFindingResult Result = NavSys->FindPathSync(Query);
	if (!Result.IsSuccessful() || !Result.Path.IsValid())
	{
		return {};
	}

	return static_cast<float>(Result.Path->GetLength());
}
