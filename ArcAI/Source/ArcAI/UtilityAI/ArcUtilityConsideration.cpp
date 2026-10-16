// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/ArcUtilityConsideration.h"
#include "Mass/EntityManager.h"

FArcUtilityConsiderationResult FArcUtilityConsideration_Distance::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	const FVector TargetLoc = Target.GetLocation(Context.EntityManager);
	const float Dist = FVector::Dist(Context.QuerierLocation, TargetLoc);
	return FArcUtilityConsiderationResult::Accept(FMath::Clamp(1.0f - (Dist / MaxDistance), 0.0f, 1.0f));
}

FArcUtilityConsiderationResult FArcUtilityConsideration_Constant::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	return FArcUtilityConsiderationResult::Accept(Value);
}
