// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"
#include "Tasks/ArcMassNavMeshPathFollowTask.h"

#include "ArcSmartObjectPlanPathLengthScorer.generated.h"

/**
 * Scores candidates based on navigation path length from the requesting entity.
 * Shorter paths receive higher scores. Returns 0 if no path exists.
 */
USTRUCT(BlueprintType, DisplayName = "Path Length Scorer")
struct FArcSmartObjectPlanPathLengthScorer : public FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	/** Maximum path length for scoring normalization. Paths longer than this get score 0. */
	UPROPERTY(EditAnywhere)
	float MaxPathLength = 5000.f;

	/** Navigation agent properties to use for pathfinding. Uses default if not set. */
	UPROPERTY(EditAnywhere)
	FNavAgentProperties NavAgentProperties;

	virtual float ScoreEntity(const FArcPotentialEntity& Entity,
							  const FMassEntityManager& EntityManager) const override;
};
