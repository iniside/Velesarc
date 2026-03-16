// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_KnowledgeFreshness.generated.h"

/**
 * Scores TQS items by knowledge entry freshness.
 * Newer entries score higher with exponential half-life decay.
 */
USTRUCT(DisplayName = "Knowledge Freshness")
struct ARCKNOWLEDGE_API FArcTQSStep_KnowledgeFreshness : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_KnowledgeFreshness() { StepType = EArcTQSStepType::Score; }

	/** Half-life in seconds. After this duration, the score drops to 0.5. */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.01))
	float HalfLifeSeconds = 300.0f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
