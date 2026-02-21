// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"

#include "ArcSmartObjectPlanRandomScorer.generated.h"

/**
 * Assigns a random score from a configurable range to each candidate.
 * Useful for introducing variety in plan selection.
 */
USTRUCT(BlueprintType, DisplayName = "Random Scorer")
struct FArcSmartObjectPlanRandomScorer : public FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	/** Minimum random score. */
	UPROPERTY(EditAnywhere)
	float MinScore = 0.f;

	/** Maximum random score. */
	UPROPERTY(EditAnywhere)
	float MaxScore = 1.f;

	virtual float ScoreEntity(const FArcPotentialEntity& Entity,
							  const FMassEntityManager& EntityManager) const override;
};
