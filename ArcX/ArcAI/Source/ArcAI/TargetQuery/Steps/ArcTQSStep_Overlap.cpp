// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Overlap.h"
#include "Engine/World.h"

float FArcTQSStep_Overlap::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	UWorld* World = QueryContext.World.Get();
	if (!World)
	{
		return 0.0f;
	}

	const FVector TestLocation = Item.GetLocation(QueryContext.EntityManager) + FVector(0.0f, 0.0f, HeightOffset);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ArcTQSOverlap), false);

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

	const bool bHasOverlap = World->OverlapAnyTestByChannel(
		TestLocation,
		FQuat::Identity,
		CollisionChannel,
		FCollisionShape::MakeSphere(OverlapRadius),
		Params);

	const bool bPass = (bHasOverlap == bPassOnOverlap);
	return bPass ? 1.0f : 0.0f;
}
