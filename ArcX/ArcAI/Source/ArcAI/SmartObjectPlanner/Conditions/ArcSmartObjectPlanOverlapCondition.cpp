// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanOverlapCondition.h"

#include "CollisionQueryParams.h"
#include "MassEntityManager.h"
#include "Engine/World.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanOverlapCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FMassEntityManager& EntityManager) const
{
	const UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Center = Entity.Location + FVector(0.f, 0.f, HeightOffset);
	const FCollisionShape Shape = FCollisionShape::MakeSphere(Radius);

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	const bool bHasOverlap = World->OverlapBlockingTestByChannel(Center, FQuat::Identity, CollisionChannel, Shape, Params);

	return bHasOverlap == bInvert;
}
