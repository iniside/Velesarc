// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_EntityHealth.h"
#include "MassEntityManager.h"
#include "ArcMassDamageSystem/ArcMassHealthStatsFragment.h"

float FArcUtilityConsideration_EntityHealth::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	if (!Context.EntityManager || !Target.EntityHandle.IsSet())
	{
		return 0.0f;
	}

	if (!Context.EntityManager->IsEntityValid(Target.EntityHandle))
	{
		return 0.0f;
	}

	const FArcMassHealthStatsFragment* HealthFragment =
		Context.EntityManager->GetFragmentDataPtr<FArcMassHealthStatsFragment>(Target.EntityHandle);

	const float MaxHealth = HealthFragment ? HealthFragment->MaxHealth.GetCurrentValue() : 0.0f;
	if (!HealthFragment || MaxHealth <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(HealthFragment->Health.GetCurrentValue() / MaxHealth, 0.0f, 1.0f);
}
