// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/Considerations/ArcUtilityConsideration_Path.h"
#include "Mass/EntityManager.h"
#include "ArcMass/Helpers/ArcNavHelpers.h"

FArcUtilityConsiderationResult FArcUtilityConsideration_PathLength::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	const UWorld* World = Context.World.Get();
	const FVector TargetLoc = Target.GetLocation(Context.EntityManager);

	const TOptional<float> Len = ArcNavHelpers::PathLength(World, Context.QuerierLocation, TargetLoc);
	if (!Len.IsSet())
	{
		return FArcUtilityConsiderationResult::Accept(0.0f);
	}

	const float PathLength = Len.GetValue();
	return FArcUtilityConsiderationResult::Accept(FMath::Clamp(1.0f - (PathLength / MaxPathLength), 0.0f, 1.0f));
}

FArcUtilityConsiderationResult FArcUtilityConsideration_PathExists::Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const
{
	const UWorld* World = Context.World.Get();
	const FVector TargetLoc = Target.GetLocation(Context.EntityManager);

	return FArcUtilityConsiderationResult::Accept(ArcNavHelpers::PathExists(World, Context.QuerierLocation, TargetLoc) ? 1.0f : 0.0f);
}
