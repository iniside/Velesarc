// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"
#include "Tasks/ArcMassNavMeshPathFollowTask.h"

#include "ArcSmartObjectPlanPathfindingCondition.generated.h"

/**
 * Tests whether a navigation path exists from the requesting entity's location to the candidate item.
 * Fails if no valid path can be found.
 */
USTRUCT(BlueprintType, DisplayName = "Pathfinding Test")
struct FArcSmartObjectPlanPathfindingCondition : public FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	/** Navigation agent properties to use for pathfinding. Uses default if not set. */
	UPROPERTY(EditAnywhere)
	FNavAgentProperties NavAgentProperties;

	virtual bool CanUseEntity(const FArcPotentialEntity& Entity,
							  const FMassEntityManager& EntityManager) const override;
};
