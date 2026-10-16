// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Overlap.h"
#include "ArcMass/Helpers/ArcCollisionHelpers.h"

float FArcTQSStep_Overlap::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return UnscorableRaw;
	}

	const FVector TestLocation = Item.GetLocation(QueryContext.EntityManager) + FVector(0.0f, 0.0f, HeightOffset);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTQSOverlap), false);

	if (const AActor* Ignored = IgnoredActor.Get())
	{
		Params.AddIgnoredActor(Ignored);
	}

	// Ignore target actor if available
	if (Item.Actor.IsValid())
	{
		Params.AddIgnoredActor(Item.Actor.Get());
	}

	const bool bHasOverlap = ArcCollisionHelpers::OverlapAnySphere(World, TestLocation, OverlapRadius, CollisionChannel, Params);

	const bool bPass = (bHasOverlap == bPassOnOverlap);
	return bPass ? 1.0f : 0.0f;
}
