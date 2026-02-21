// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanHealthCondition.h"

#include "ArcMass/ArcMassFragments.h"
#include "MassEntityManager.h"
#include "ArcMass/ArcMassFragments.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanHealthCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FMassEntityManager& EntityManager) const
{
	const FMassEntityHandle TargetEntity = bTestRequestingEntity
		? Entity.RequestingEntity
		: Entity.EntityHandle;

	const FArcMassHealthFragment* HealthFragment = EntityManager.GetFragmentDataPtr<FArcMassHealthFragment>(TargetEntity);
	if (!HealthFragment)
	{
		return false;
	}

	const float HealthPercent = (HealthFragment->MaxHealth > 0.f)
		? (HealthFragment->CurrentHealth / HealthFragment->MaxHealth)
		: 0.f;

	switch (Comparison)
	{
	case EArcHealthComparison::GreaterThan:
		return HealthPercent > HealthThreshold;
	case EArcHealthComparison::LessThan:
		return HealthPercent < HealthThreshold;
	case EArcHealthComparison::GreaterOrEqual:
		return HealthPercent >= HealthThreshold;
	case EArcHealthComparison::LessOrEqual:
		return HealthPercent <= HealthThreshold;
	default:
		return false;
	}
}
