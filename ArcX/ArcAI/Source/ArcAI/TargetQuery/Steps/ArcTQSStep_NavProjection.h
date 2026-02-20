// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_NavProjection.generated.h"

/**
 * Projects each item's location onto the navmesh.
 * Items that cannot be projected are filtered out.
 * Items that can be projected have their location updated to the projected point.
 *
 * This is a Filter step: items off the navmesh are discarded.
 * Note: This modifies item locations as a side effect.
 */
USTRUCT(DisplayName = "NavMesh Projection")
struct ARCAI_API FArcTQSStep_NavProjection : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_NavProjection()
	{
		StepType = EArcTQSStepType::Filter;
	}

	// Vertical extent for the projection query
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0))
	float ProjectionExtent = 500.0f;

	// Horizontal extent for the projection query (0 = vertical only)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0))
	float HorizontalExtent = 0.0f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
