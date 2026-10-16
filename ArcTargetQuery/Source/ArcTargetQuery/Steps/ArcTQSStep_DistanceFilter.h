// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_DistanceFilter.generated.h"

/**
 * Hard distance filter. Discards items outside the specified range from a reference location.
 */
USTRUCT(DisplayName = "Distance Filter")
struct ARCTARGETQUERY_API FArcTQSStep_DistanceFilter : public FArcTQSStep
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

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FVector ReferenceLocation = FVector::ZeroVector;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
