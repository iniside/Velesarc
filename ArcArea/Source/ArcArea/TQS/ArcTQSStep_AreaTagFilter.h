// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_AreaTagFilter.generated.h"

/**
 * Filters TQS items by area gameplay tags.
 * Reads AreaHandle from ItemData and checks the area's tags.
 */
USTRUCT(DisplayName = "Area Tag Filter")
struct ARCAREA_API FArcTQSStep_AreaTagFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_AreaTagFilter() { StepType = EArcTQSStepType::Filter; }

	UPROPERTY(EditAnywhere, Category = "Step")
	FGameplayTagQuery TagQuery;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
