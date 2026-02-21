// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Math/Vector.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"

#include "ArcSmartObjectPlanProjectionCondition.generated.h"

/**
 * Projects the candidate item location onto the navmesh.
 * Fails if the location cannot be projected within the specified extent.
 */
USTRUCT(BlueprintType, DisplayName = "NavMesh Projection Test")
struct FArcSmartObjectPlanProjectionCondition : public FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	/** Search extent for the navmesh projection. */
	UPROPERTY(EditAnywhere)
	FVector ProjectionExtent = FVector(100.f, 100.f, 250.f);

	virtual bool CanUseEntity(const FArcPotentialEntity& Entity,
							  const FMassEntityManager& EntityManager) const override;
};
