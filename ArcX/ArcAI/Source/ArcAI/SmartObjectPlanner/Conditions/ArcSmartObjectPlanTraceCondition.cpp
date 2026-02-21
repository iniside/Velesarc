// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanTraceCondition.h"

#include "CollisionQueryParams.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanTraceCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FMassEntityManager& EntityManager) const
{
	const UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return false;
	}

	// Get the requesting entity's location
	const FTransformFragment* RequestingTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(Entity.RequestingEntity);
	if (!RequestingTransform)
	{
		return false;
	}

	const FVector HeightOffsetVec(0.f, 0.f, HeightOffset);
	const FVector Start = RequestingTransform->GetTransform().GetLocation() + HeightOffsetVec;
	const FVector End = Entity.Location + HeightOffsetVec;

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, TraceChannel, Params);

	return bHit == bInvert;
}
