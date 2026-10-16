// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanOverlapCondition.h"

#include "CollisionQueryParams.h"
#include "Mass/EntityManager.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"
#include "ArcMass/Helpers/ArcCollisionHelpers.h"

bool FArcSmartObjectPlanOverlapCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context) const
{
	const UWorld* World = Context.EntityManager->GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Center = Entity.Location + FVector(0.f, 0.f, HeightOffset);

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	const bool bHasOverlap = ArcCollisionHelpers::OverlapBlockingSphere(World, Center, Radius, CollisionChannel, Params);

	return bHasOverlap == bInvert;
}
