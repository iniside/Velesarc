// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_Angle.h"
#include "MassEntityManager.h"

float FArcUtilityConsideration_Angle::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	const FVector TargetLoc = Target.GetLocation(Context.EntityManager);
	const FVector Direction = (TargetLoc - Context.QuerierLocation).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		return 0.0f;
	}

	const float Dot = FVector::DotProduct(Context.QuerierForward, Direction);

	// Convert dot product [-1,1] to angle in degrees [0,180]
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));

	if (AngleDeg >= MaxAngleDegrees)
	{
		return 0.0f;
	}

	return 1.0f - (AngleDeg / MaxAngleDegrees);
}
