// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSLocationProvider.h"
#include "ArcTQSStep_Distance.generated.h"

/**
 * Distance-based scoring step.
 * Produces a 0-1 normalized score based on distance from a reference location to the item.
 *
 * When the location provider returns multiple locations, the item is scored against
 * each one and the best (highest) score is used.
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

	/** Reference location configuration for distance measurement. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FArcTQSLocationConfig LocationConfig;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
