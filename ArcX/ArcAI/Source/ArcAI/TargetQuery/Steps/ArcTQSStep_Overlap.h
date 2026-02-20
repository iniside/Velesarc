// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_Overlap.generated.h"

/**
 * Simple overlap test at each item's location.
 * Performs a sphere overlap check and passes/fails based on whether any overlap was found.
 *
 * As a Filter: items with overlaps are discarded (or kept, depending on bPassOnOverlap).
 * As a Score: returns 1.0 if overlap matches expected result, 0.0 otherwise.
 */
USTRUCT(DisplayName = "Overlap Test")
struct ARCAI_API FArcTQSStep_Overlap : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Overlap()
	{
		StepType = EArcTQSStepType::Filter;
	}

	// Radius of the overlap sphere
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 1.0))
	float OverlapRadius = 50.0f;

	// Height offset for the overlap test center
	UPROPERTY(EditAnywhere, Category = "Step")
	float HeightOffset = 50.0f;

	// Collision channel for the overlap test
	UPROPERTY(EditAnywhere, Category = "Step")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_Pawn;

	// If true, items that DO overlap pass. If false, items that do NOT overlap pass.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bPassOnOverlap = false;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
