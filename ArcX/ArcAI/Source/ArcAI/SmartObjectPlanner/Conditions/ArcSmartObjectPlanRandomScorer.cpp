// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanRandomScorer.h"

#include "SmartObjectPlanner/ArcPotentialEntity.h"

float FArcSmartObjectPlanRandomScorer::ScoreEntity(
	const FArcPotentialEntity& Entity,
	const FArcSmartObjectPlanEvaluationContext& Context) const
{
	return FMath::FRandRange(MinScore, MaxScore);
}
