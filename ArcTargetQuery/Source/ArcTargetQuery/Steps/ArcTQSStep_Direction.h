// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_Direction.generated.h"

/**
 * Scores or filters items based on directional alignment.
 *
 * Computes how well the direction from Origin to each item aligns with
 * the direction from Origin to ReferenceLocation. Uses dot product
 * mapped from [-1, 1] to [0, 1].
 */
USTRUCT(DisplayName = "Direction Score")
struct ARCTARGETQUERY_API FArcTQSStep_Direction : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Direction()
	{
		StepType = EArcTQSStepType::Score;
	}

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FVector ReferenceLocation = FVector::ZeroVector;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
