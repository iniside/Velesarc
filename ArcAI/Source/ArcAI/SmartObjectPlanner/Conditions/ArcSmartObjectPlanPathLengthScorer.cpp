// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanPathLengthScorer.h"

#include "Mass/EntityManager.h"
#include "ArcMass/Helpers/ArcNavHelpers.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

float FArcSmartObjectPlanPathLengthScorer::ScoreEntity(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context) const
{
	const UWorld* World = Context.EntityManager->GetWorld();

	const TOptional<float> Len = ArcNavHelpers::PathLength(World, Context.RequestingLocation, Entity.Location, NavAgentProperties);
	if (!Len.IsSet() || MaxPathLength <= 0.f)
	{
		return 0.f;
	}

	const float PathLength = Len.GetValue();
	if (PathLength >= MaxPathLength)
	{
		return 0.f;
	}

	// Shorter paths get higher scores (1.0 = at origin, 0.0 = at max distance)
	return 1.f - (PathLength / MaxPathLength);
}
