// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanHealthCondition.h"

#include "ArcMassDamageSystem/ArcMassHealthStatsFragment.h"
#include "MassEntityManager.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanHealthCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context) const
{
	const FMassEntityHandle TargetEntity = bTestRequestingEntity
		? Context.RequestingEntity
		: Entity.EntityHandle;

	const FArcMassHealthStatsFragment* HealthFragment = Context.EntityManager->GetFragmentDataPtr<FArcMassHealthStatsFragment>(TargetEntity);
	if (!HealthFragment)
	{
		return false;
	}

	const float MaxHealth = HealthFragment->MaxHealth.GetCurrentValue();
	const float HealthPercent = (MaxHealth > 0.f)
		? (HealthFragment->Health.GetCurrentValue() / MaxHealth)
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
