// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_Attribute.generated.h"

/**
 * Scores items based on a Gameplay Attribute value on the target actor.
 * Accesses UAbilitySystemComponent via IAbilitySystemInterface on the resolved actor.
 *
 * The attribute value is normalized to [0,1] either by a manually specified MaxValue
 * or by reading another Gameplay Attribute from the same actor (e.g. MaxHealth for Health).
 *
 * Score = clamp(AttributeValue / MaxValue, 0, 1) → ResponseCurve
 */
USTRUCT(DisplayName = "Attribute Score")
struct ARCAI_API FArcTQSStep_Attribute : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Attribute()
	{
		StepType = EArcTQSStepType::Score;
	}

	// The gameplay attribute to read from the target actor
	UPROPERTY(EditAnywhere, Category = "Step")
	FGameplayAttribute Attribute;

	// If true, use MaxAttribute to read the max value from the same actor. If false, use ManualMaxValue.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bUseMaxAttribute = false;

	// Attribute to use as the max value for normalization (e.g. MaxHealth)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "bUseMaxAttribute"))
	FGameplayAttribute MaxAttribute;

	// Manual max value for normalization when not using a max attribute
	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "!bUseMaxAttribute", ClampMin = 0.01))
	float ManualMaxValue = 100.0f;

	// If true, items without a resolvable actor or ASC are filtered out
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bFilterItemsWithoutAttribute = false;

	// Score for items that don't have the attribute (when not filtering)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0, ClampMax = 1.0, EditCondition = "!bFilterItemsWithoutAttribute"))
	float DefaultScore = 0.5f;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
