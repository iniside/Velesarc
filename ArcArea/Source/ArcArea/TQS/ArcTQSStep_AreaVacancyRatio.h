// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_AreaVacancyRatio.generated.h"

/**
 * Scores TQS items by the area's vacancy ratio (vacant slots / total slots).
 * Higher vacancy = higher score by default. Set bInvert for busier areas to score higher.
 */
USTRUCT(DisplayName = "Area Vacancy Ratio")
struct ARCAREA_API FArcTQSStep_AreaVacancyRatio : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_AreaVacancyRatio() { StepType = EArcTQSStepType::Score; }

	/** If true, fewer vacancies = higher score (find busy areas). */
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bInvert = false;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
