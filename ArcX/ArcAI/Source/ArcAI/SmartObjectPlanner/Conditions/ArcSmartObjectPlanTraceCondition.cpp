// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanTraceCondition.h"

#include "CollisionQueryParams.h"
#include "MassEntityManager.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanTraceCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context) const
{
	const UWorld* World = Context.EntityManager->GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector HeightOffsetVec(0.f, 0.f, HeightOffset);
	const FVector Start = Context.RequestingLocation + HeightOffsetVec;
	const FVector End = Entity.Location + HeightOffsetVec;

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, TraceChannel, Params);

	return bHit == bInvert;
}
