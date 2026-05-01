// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_KnowledgeAgeFilter.generated.h"

/**
 * Filters TQS items by knowledge entry age.
 * Discards entries outside the [MinAge, MaxAge] range in seconds.
 */
USTRUCT(DisplayName = "Knowledge Age Filter")
struct ARCKNOWLEDGE_API FArcTQSStep_KnowledgeAgeFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_KnowledgeAgeFilter() { StepType = EArcTQSStepType::Filter; }

	/** Minimum age in seconds. 0 = no minimum. */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0))
	float MinAge = 0.0f;

	/** Maximum age in seconds. 0 = no maximum. */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0))
	float MaxAge = 0.0f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
