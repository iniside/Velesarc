// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_PathLength.h"
#include "TargetQuery/ArcTQSLocationProvider.h"
#include "NavigationSystem.h"
#include "NavigationData.h"

float FArcTQSStep_PathLength::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryContext.World.Get());
	if (!NavSys)
	{
		return 0.0f;
	}

	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		return 0.0f;
	}

	// Resolve start locations
	TArray<FVector> StartLocations;
	if (bUseLocationProvider)
	{
		if (const FArcTQSLocationProvider* Provider = LocationProvider.GetPtr<FArcTQSLocationProvider>())
		{
			Provider->GetLocations(Item, QueryContext, StartLocations);
		}
		else
		{
			StartLocations.Add(QueryContext.ContextLocations.IsValidIndex(Item.ContextIndex)
				? QueryContext.ContextLocations[Item.ContextIndex]
				: QueryContext.QuerierLocation);
		}
	}
	else
	{
		StartLocations.Add(PathStart);
	}

	const FVector EndLocation = Item.GetLocation(QueryContext.EntityManager);

	// Score against all start locations, pick the best (highest score)
	float BestScore = 0.0f;
	for (const FVector& StartLoc : StartLocations)
	{
		FPathFindingQuery Query(nullptr, *NavData, StartLoc, EndLocation);
		Query.bAllowPartialPaths = false;

		FPathFindingResult Result = NavSys->FindPathSync(Query);
		if (!Result.IsSuccessful() || !Result.Path.IsValid())
		{
			continue;
		}

		const float PathLength = static_cast<float>(Result.Path->GetLength());
		const float NormalizedLength = FMath::Clamp(PathLength / MaxPathLength, 0.0f, 1.0f);
		const float Score = ResponseCurve.Evaluate(NormalizedLength);
		BestScore = FMath::Max(BestScore, Score);
	}

	return BestScore;
}
