// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_LineOfSight.h"
#include "Mass/EntityManager.h"
#include "CollisionQueryParams.h"
#include "ArcMass/Helpers/ArcCollisionHelpers.h"

FArcUtilityConsiderationResult FArcUtilityConsideration_LineOfSight::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	UWorld* World = Context.World.Get();
	if (!World)
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	const FVector HeightOffset(0.0f, 0.0f, TraceHeightOffset);
	const FVector Start = Context.QuerierLocation + HeightOffset;
	const FVector End = Target.GetLocation(Context.EntityManager) + HeightOffset;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(UtilityLOS), /*bTraceComplex=*/ false);

	// Ignore the querier actor
	if (AActor* QuerierActor = Context.QuerierActor.Get())
	{
		Params.AddIgnoredActor(QuerierActor);
	}

	// Ignore the target actor if available
	if (AActor* TargetActor = Target.GetActor(Context.EntityManager))
	{
		Params.AddIgnoredActor(TargetActor);
	}

	FHitResult Hit;
	const bool bBlocked = ArcCollisionHelpers::LineTrace(World, Hit, Start, End, TraceChannel, Params);

	return FArcUtilityConsiderationResult::Accept(bBlocked ? 0.0f : 1.0f);
}
