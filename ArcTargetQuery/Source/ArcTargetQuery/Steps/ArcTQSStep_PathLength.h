// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSLocationProvider.h"
#include "ArcTQSStep_PathLength.generated.h"

/**
 * Scores items based on navigation path length from a reference location to the item.
 * Uses FindPathSync to compute the actual path and normalizes the length against MaxPathLength.
 *
 * Items with no valid path get score 0.
 */
USTRUCT(DisplayName = "Path Length Score")
struct ARCTARGETQUERY_API FArcTQSStep_PathLength : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_PathLength()
	{
		StepType = EArcTQSStepType::Score;
	}

	// Maximum path length for normalization. Paths longer than this score 0 (if bPreferShorter) or 1.
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 1.0))
	float MaxPathLength = 10000.0f;

	/** Path start location configuration. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FArcTQSLocationConfig LocationConfig;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
