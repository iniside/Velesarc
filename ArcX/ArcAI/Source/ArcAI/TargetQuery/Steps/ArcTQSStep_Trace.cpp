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

	// Resolve trace origin
	FVector Origin;
	if (bUseLocationProvider)
	{
		if (const FArcTQSLocationProvider* Provider = LocationProvider.GetPtr<FArcTQSLocationProvider>())
		{
			Origin = Provider->GetLocation(Item, QueryContext);
		}
		else
		{
			// Default: use item's context location if available, otherwise querier
			Origin = QueryContext.ContextLocations.IsValidIndex(Item.ContextIndex)
				? QueryContext.ContextLocations[Item.ContextIndex]
				: QueryContext.QuerierLocation;
		}
	}
	else
	{
		Origin = TraceOrigin;
	}

	const FVector HeightOffsetVec(0.0f, 0.0f, TraceHeightOffset);
	const FVector TraceStart = Origin + HeightOffsetVec;
	const FVector TraceEnd = Item.GetLocation(QueryContext.EntityManager) + HeightOffsetVec;

	FHitResult HitResult;
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

	const bool bHit = World->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, TraceChannel, Params);

	// No hit = clear line of sight = pass
	const float RawScore = bHit ? 0.0f : 1.0f;
	return ResponseCurve.Evaluate(RawScore);
}
