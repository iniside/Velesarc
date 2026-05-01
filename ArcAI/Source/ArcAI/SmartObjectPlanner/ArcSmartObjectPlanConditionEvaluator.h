#pragma once

#include "Mass/EntityHandle.h"
#include "Math/Vector.h"

#include "ArcSmartObjectPlanConditionEvaluator.generated.h"

struct FArcPotentialEntity;
struct FMassEntityManager;

/** Context describing who is requesting the smart object evaluation. Built once per planning cycle. */
USTRUCT()
struct FArcSmartObjectPlanEvaluationContext
{
	GENERATED_BODY()

	FMassEntityHandle RequestingEntity;
	FVector RequestingLocation = FVector::ZeroVector;
	const FMassEntityManager* EntityManager = nullptr;
};

USTRUCT(BlueprintType)
struct FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	virtual bool CanUseEntity(const FArcPotentialEntity& Entity,
						 const FArcSmartObjectPlanEvaluationContext& Context) const { return true; }

	virtual float ScoreEntity(const FArcPotentialEntity& Entity,
						 const FArcSmartObjectPlanEvaluationContext& Context) const { return 1.0f; }

	virtual ~FArcSmartObjectPlanConditionEvaluator() = default;
};
