// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"

#include "ArcSmartObjectPlanTraceCondition.generated.h"

/**
 * Tests line-of-sight from the requesting entity's location to the candidate item location.
 * Fails if the trace is blocked.
 */
USTRUCT(BlueprintType, DisplayName = "Trace Test")
struct FArcSmartObjectPlanTraceCondition : public FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	/** Trace channel to use. */
	UPROPERTY(EditAnywhere)
	TEnumAsByte<ECollisionChannel> TraceChannel;

	/** Height offset applied to both start and end points. */
	UPROPERTY(EditAnywhere)
	float HeightOffset = 50.f;

	/** If true, pass when trace is blocked (inverts the result). */
	UPROPERTY(EditAnywhere)
	bool bInvert = false;

	virtual bool CanUseEntity(const FArcPotentialEntity& Entity,
							  const FMassEntityManager& EntityManager) const override;
};
