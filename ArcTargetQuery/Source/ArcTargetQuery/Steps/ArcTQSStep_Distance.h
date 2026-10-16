// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_Distance.generated.h"

/**
 * Distance-based scoring step.
 * Produces a 0-1 normalized score based on distance from a reference location to the item.
 */
USTRUCT(DisplayName = "Distance Score")
struct ARCTARGETQUERY_API FArcTQSStep_Distance : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Distance()
	{
		StepType = EArcTQSStepType::Score;
	}

	// Maximum distance for normalization. Items beyond this get score 0 (if bPreferCloser) or 1.
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 1.0))
	float MaxDistance = 5000.0f;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FVector ReferenceLocation = FVector::ZeroVector;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
