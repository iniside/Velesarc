// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "Attributes/ArcAttribute.h"
#include "ArcTQSStep_Health.generated.h"

/**
 * Scores or filters items based on the target entity's health.
 * Reads designer-configured Health and MaxHealth attributes (FArcAttributeRef) from the
 * Mass entity and produces a normalized score based on Health / MaxHealth.
 *
 * Only works with MassEntity or SmartObject target types that have an entity handle.
 * Items without a valid entity or resolvable attributes pass through (configurable).
 */
USTRUCT(DisplayName = "Health Score")
struct ARCTARGETQUERY_API FArcTQSStep_Health : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Health()
	{
		StepType = EArcTQSStepType::Score;
	}

	/** Attribute ref for the entity's current-health attribute. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FArcAttributeRef HealthAttribute;

	/** Attribute ref for the entity's max-health attribute. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FArcAttributeRef MaxHealthAttribute;

	// If true, items without health data are filtered out. If false, they get a default score.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bFilterItemsWithoutHealth = false;

	// Score assigned to items that have no resolvable health attributes (when bFilterItemsWithoutHealth is false)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0, ClampMax = 1.0, EditCondition = "!bFilterItemsWithoutHealth"))
	float DefaultScoreForNoHealth = 0.5f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
