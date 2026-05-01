// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSStep_Health.h"
#include "ArcMassDamageSystem/ArcMassHealthStatsFragment.h"
#include "MassEntityManager.h"

float FArcTQSStep_Health::ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const
{
	if (!QueryContext.EntityManager || !Item.EntityHandle.IsValid())
	{
		return bFilterItemsWithoutHealth ? 0.0f : DefaultScoreForNoHealth;
	}

	if (!QueryContext.EntityManager->IsEntityValid(Item.EntityHandle))
	{
		return bFilterItemsWithoutHealth ? 0.0f : DefaultScoreForNoHealth;
	}

	const FArcMassHealthStatsFragment* HealthFragment =
		QueryContext.EntityManager->GetFragmentDataPtr<FArcMassHealthStatsFragment>(Item.EntityHandle);

	if (!HealthFragment)
	{
		return bFilterItemsWithoutHealth ? 0.0f : DefaultScoreForNoHealth;
	}

	const float MaxHealth = HealthFragment->MaxHealth.GetCurrentValue();
	if (MaxHealth <= 0.0f)
	{
		return bFilterItemsWithoutHealth ? 0.0f : DefaultScoreForNoHealth;
	}

	const float HealthRatio = FMath::Clamp(HealthFragment->Health.GetCurrentValue() / MaxHealth, 0.0f, 1.0f);
	return ResponseCurve.Evaluate(HealthRatio);
}
