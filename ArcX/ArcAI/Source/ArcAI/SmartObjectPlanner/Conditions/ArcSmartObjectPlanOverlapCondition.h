// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "SmartObjectPlanner/ArcSmartObjectPlanConditionEvaluator.h"

#include "ArcSmartObjectPlanOverlapCondition.generated.h"

/**
 * Simple overlap test at the candidate item location.
 * Fails if the overlap detects any blocking hit.
 */
USTRUCT(BlueprintType, DisplayName = "Overlap Test")
struct FArcSmartObjectPlanOverlapCondition : public FArcSmartObjectPlanConditionEvaluator
{
	GENERATED_BODY()

	/** Overlap sphere radius. */
	UPROPERTY(EditAnywhere)
	float Radius = 50.f;

	/** Collision channel to test against. */
	UPROPERTY(EditAnywhere)
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Pawn;

	/** Height offset for the overlap center. */
	UPROPERTY(EditAnywhere)
	float HeightOffset = 0.f;

	/** If true, pass when overlap IS blocked (inverts the result). */
	UPROPERTY(EditAnywhere)
	bool bInvert = false;

	virtual bool CanUseEntity(const FArcPotentialEntity& Entity,
							  const FMassEntityManager& EntityManager) const override;
};
