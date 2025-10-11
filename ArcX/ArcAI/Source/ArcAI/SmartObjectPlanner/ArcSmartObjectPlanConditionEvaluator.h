#pragma once

#include "ArcSmartObjectPlanConditionEvaluator.generated.h"

struct FArcPotentialEntity;
struct FMassEntityManager;

USTRUCT(BlueprintType)
struct FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	virtual bool CanUseEntity(const FArcPotentialEntity& Entity, 
						 const FMassEntityManager& EntityManager) const { return true; }
    
	virtual float ScoreEntity(const FArcPotentialEntity& Entity,
						 const FMassEntityManager& EntityManager) const { return 1.0f; }

	virtual ~FArcSmartObjectPlanConditionEvaluator() = default;
};