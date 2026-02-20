// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_Health.generated.h"

/**
 * Scores or filters items based on the target entity's health.
 * Reads FArcHealthFragment from the Mass Entity and produces a normalized score
 * based on the health ratio (Health / MaxHealth).
 *
 * Only works with MassEntity or SmartObject target types that have an entity handle.
 * Items without a valid entity or health fragment pass through (configurable).
 */
USTRUCT(DisplayName = "Health Score")
struct ARCAI_API FArcTQSStep_Health : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Health()
	{
		StepType = EArcTQSStepType::Score;
	}

	// If true, items without health data are filtered out. If false, they get a default score.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bFilterItemsWithoutHealth = false;

	// Score assigned to items that have no health fragment (when bFilterItemsWithoutHealth is false)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0, ClampMax = 1.0, EditCondition = "!bFilterItemsWithoutHealth"))
	float DefaultScoreForNoHealth = 0.5f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
