// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "TargetQuery/ArcTQSLocationProvider.h"
#include "ArcTQSStep_DistanceFilter.generated.h"

/**
 * Hard distance filter. Discards items outside the specified range from a reference location.
 *
 * When the location provider returns multiple locations, the item passes if it is
 * within range of ANY of the returned locations.
 */
USTRUCT(DisplayName = "Distance Filter")
struct ARCAI_API FArcTQSStep_DistanceFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_DistanceFilter()
	{
		StepType = EArcTQSStepType::Filter;
	}

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0))
	float MinDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0))
	float MaxDistance = 5000.0f;

	/** Reference location configuration for distance filtering. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FArcTQSLocationConfig LocationConfig;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
