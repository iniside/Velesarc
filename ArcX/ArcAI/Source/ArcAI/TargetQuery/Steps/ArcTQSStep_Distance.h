// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_Distance.generated.h"

/**
 * Distance-based scoring step.
 * Produces a 0-1 normalized score based on distance from querier.
 */
USTRUCT(DisplayName = "Distance Score")
struct ARCAI_API FArcTQSStep_Distance : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Distance()
	{
		StepType = EArcTQSStepType::Score;
	}

	// If true, closer items score higher. If false, farther items score higher.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bPreferCloser = true;

	// Maximum distance for normalization. Items beyond this get score 0 (if bPreferCloser) or 1.
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 1.0))
	float MaxDistance = 5000.0f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
