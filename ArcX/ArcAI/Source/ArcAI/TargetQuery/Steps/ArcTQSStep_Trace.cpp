// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Trace.h"
#include "TargetQuery/ArcTQSLocationProvider.h"
#include "Engine/World.h"

float FArcTQSStep_Trace::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return 0.0f;
	}

	// Resolve trace origins
	TArray<FVector> Origins;
	if (bUseLocationProvider)
	{
		if (const FArcTQSLocationProvider* Provider = LocationProvider.GetPtr<FArcTQSLocationProvider>())
		{
			Provider->GetLocations(Item, QueryContext, Origins);
		}
		else
		{
			Origins.Add(QueryContext.ContextLocations.IsValidIndex(Item.ContextIndex)
				? QueryContext.ContextLocations[Item.ContextIndex]
				: QueryContext.QuerierLocation);
		}
	}
	else
	{
		Origins.Add(TraceOrigin);
	}

	const FVector HeightOffsetVec(0.0f, 0.0f, TraceHeightOffset);
	const FVector TraceEnd = Item.GetLocation(QueryContext.EntityManager) + HeightOffsetVec;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTQSTrace), true);

	// Ignore querier actor
	if (const AActor* QuerierActor = QueryContext.QuerierActor.Get())
	{
		Params.AddIgnoredActor(QuerierActor);
	}

	// Ignore target actor if available
	if (Item.Actor.IsValid())
	{
		Params.AddIgnoredActor(Item.Actor.Get());
	}

	// Score against all origins, pick the best (highest score).
	// For filters: pass if visible from ANY origin.
	float BestScore = 0.0f;
	for (const FVector& Origin : Origins)
	{
		const FVector TraceStart = Origin + HeightOffsetVec;

		FHitResult HitResult;
		const bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, TraceChannel, Params);

		const float RawScore = bHit ? 0.0f : 1.0f;
		const float Score = ResponseCurve.Evaluate(RawScore);
		BestScore = FMath::Max(BestScore, Score);

		// Early out: if clear line of sight from any origin, no need to check more
		if (BestScore >= 1.0f)
		{
			return BestScore;
		}
	}

	return BestScore;
}
