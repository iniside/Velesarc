// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Distance.h"

float FArcTQSStep_Distance::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	const FVector ItemLocation = Item.GetLocation(QueryContext.EntityManager);

	// Resolve reference locations
	TArray<FVector> RefLocations;
	LocationConfig.ResolveLocations(Item, QueryContext, RefLocations);

	// Score against all locations, pick the best (highest score)
	float BestScore = 0.0f;
	for (const FVector& RefLoc : RefLocations)
	{
		const float Distance = FVector::Dist(RefLoc, ItemLocation);
		const float NormalizedDistance = FMath::Clamp(Distance / MaxDistance, 0.0f, 1.0f);
		const float Score = ResponseCurve.Evaluate(NormalizedDistance);
		BestScore = FMath::Max(BestScore, Score);
	}

	return BestScore;
}
