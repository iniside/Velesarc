// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_DistanceFilter.generated.h"

/**
 * Hard distance filter. Discards items outside the specified range.
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

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
