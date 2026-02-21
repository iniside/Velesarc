// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcSmartObjectPlanRandomScorer.h"

#include "SmartObjectPlanner/ArcPotentialEntity.h"

float FArcSmartObjectPlanRandomScorer::ScoreEntity(
	const FArcPotentialEntity& Entity,
	const FMassEntityManager& EntityManager) const
{
	return FMath::FRandRange(MinScore, MaxScore);
}
