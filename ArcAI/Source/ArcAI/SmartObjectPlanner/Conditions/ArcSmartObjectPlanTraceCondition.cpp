// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanTraceCondition.h"

#include "CollisionQueryParams.h"
#include "Mass/EntityManager.h"
#include "Engine/HitResult.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"
#include "ArcMass/Helpers/ArcCollisionHelpers.h"

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
	const bool bHit = ArcCollisionHelpers::LineTrace(World, HitResult, Start, End, TraceChannel, Params);

	return bHit == bInvert;
}
