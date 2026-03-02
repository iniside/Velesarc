// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_AreaVacancyFilter.generated.h"

/**
 * Filters TQS items to only pass areas with a minimum number of vacant slots.
 */
USTRUCT(DisplayName = "Area Vacancy Filter")
struct ARCAREA_API FArcTQSStep_AreaVacancyFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_AreaVacancyFilter() { StepType = EArcTQSStepType::Filter; }

	/** Minimum number of vacant slots required to pass. */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 1))
	int32 MinVacantSlots = 1;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
