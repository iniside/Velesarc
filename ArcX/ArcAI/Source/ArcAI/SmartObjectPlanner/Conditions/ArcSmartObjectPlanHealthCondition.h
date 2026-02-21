// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"

#include "ArcSmartObjectPlanHealthCondition.generated.h"

/** Comparison operator for health threshold. */
UENUM()
enum class EArcHealthComparison : uint8
{
	GreaterThan,
	LessThan,
	GreaterOrEqual,
	LessOrEqual
};

/**
 * Tests the candidate entity's health via FArcMassHealthFragment.
 * Compares health percentage against a configurable threshold.
 */
USTRUCT(BlueprintType, DisplayName = "Health Test")
struct FArcSmartObjectPlanHealthCondition : public FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	/** Health percentage threshold (0.0 - 1.0). */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthThreshold = 0.5f;

	/** How to compare the entity's health percentage against the threshold. */
	UPROPERTY(EditAnywhere)
	EArcHealthComparison Comparison = EArcHealthComparison::GreaterThan;

	/** If true, test the requesting entity's health instead of the candidate's. */
	UPROPERTY(EditAnywhere)
	bool bTestRequestingEntity = false;

	virtual bool CanUseEntity(const FArcPotentialEntity& Entity,
							  const FMassEntityManager& EntityManager) const override;
};
