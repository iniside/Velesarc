// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_Random.generated.h"

/**
 * Assigns a random score from a configurable range to each item.
 * Useful for adding randomization to the scoring pipeline or
 * as a tiebreaker when other scores are equal.
 */
USTRUCT(DisplayName = "Random Score")
struct ARCAI_API FArcTQSStep_Random : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Random()
	{
		StepType = EArcTQSStepType::Score;
	}

	// Minimum random value (before response curve)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float MinScore = 0.0f;

	// Maximum random value (before response curve)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float MaxScore = 1.0f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
