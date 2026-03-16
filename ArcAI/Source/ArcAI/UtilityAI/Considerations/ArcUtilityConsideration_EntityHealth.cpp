// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_EntityHealth.h"
#include "MassEntityManager.h"
#include "ArcMass/ArcMassFragments.h"

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

	const FArcMassHealthFragment* HealthFragment =
		Context.EntityManager->GetFragmentDataPtr<FArcMassHealthFragment>(Target.EntityHandle);

	if (!HealthFragment || HealthFragment->MaxHealth <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp(HealthFragment->CurrentHealth / HealthFragment->MaxHealth, 0.0f, 1.0f);
}
