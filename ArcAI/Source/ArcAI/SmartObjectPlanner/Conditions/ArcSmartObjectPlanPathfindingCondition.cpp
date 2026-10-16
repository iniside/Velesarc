// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanPathfindingCondition.h"

#include "Mass/EntityManager.h"
#include "ArcMass/Helpers/ArcNavHelpers.h"
#include "SmartObjectPlanner/ArcPotentialEntity.h"

bool FArcSmartObjectPlanPathfindingCondition::CanUseEntity(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context) const
{
	const UWorld* World = Context.EntityManager->GetWorld();

	return ArcNavHelpers::PathExists(World, Context.RequestingLocation, Entity.Location, NavAgentProperties);
}
