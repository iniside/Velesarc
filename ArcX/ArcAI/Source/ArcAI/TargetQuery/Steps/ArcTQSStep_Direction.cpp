// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Direction.h"
#include "TargetQuery/ArcTQSLocationProvider.h"

float FArcTQSStep_Direction::ExecuteStep(
	const FArcTQSTargetItem& Item,
	const FArcTQSQueryContext& QueryContext) const
{
	const FVector Origin = QueryContext.QuerierLocation;
	const FVector ItemLocation = Item.GetLocation(QueryContext.EntityManager);

	// Direction from origin to item
	const FVector ToItem = (ItemLocation - Origin).GetSafeNormal2D();
	if (ToItem.IsNearlyZero())
	{
		return 0.5f;
	}

	// Resolve reference locations
	TArray<FVector> RefLocations;
	if (bUseLocationProvider)
	{
		if (const FArcTQSLocationProvider* Provider = LocationProvider.GetPtr<FArcTQSLocationProvider>())
		{
			Provider->GetLocations(Item, QueryContext, RefLocations);
		}
		else
		{
			RefLocations.Add(QueryContext.QuerierLocation);
		}
	}
	else
	{
		RefLocations.Add(ReferenceLocation);
	}

	// Score against all reference locations, pick the best (highest score)
	float BestScore = 0.0f;
	for (const FVector& RefLoc : RefLocations)
	{
		const FVector ToRef = (RefLoc - Origin).GetSafeNormal2D();
		if (ToRef.IsNearlyZero())
		{
			BestScore = FMath::Max(BestScore, 0.5f);
			continue;
		}

		const float Dot = FVector::DotProduct(ToRef, ToItem);
		const float NormalizedScore = (Dot + 1.0f) * 0.5f;
		const float Score = ResponseCurve.Evaluate(NormalizedScore);
		BestScore = FMath::Max(BestScore, Score);
	}

	return BestScore;
}
