// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Trace.h"
#include "ArcMass/Helpers/ArcCollisionHelpers.h"

float FArcTQSStep_Trace::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return UnscorableRaw;
	}

	const FVector HeightOffsetVec(0.0f, 0.0f, TraceHeightOffset);
	const FVector TraceEnd = Item.GetLocation(QueryContext.EntityManager) + HeightOffsetVec;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTQSTrace), true);

	if (const AActor* Ignored = IgnoredActor.Get())
	{
		Params.AddIgnoredActor(Ignored);
	}

	// Ignore target actor if available
	if (Item.Actor.IsValid())
	{
		Params.AddIgnoredActor(Item.Actor.Get());
	}

	FHitResult HitResult;
	const bool bHit = ArcCollisionHelpers::LineTrace(World, HitResult, ReferenceLocation + HeightOffsetVec, TraceEnd, TraceChannel, Params);
	return bHit ? 0.0f : 1.0f;
}
