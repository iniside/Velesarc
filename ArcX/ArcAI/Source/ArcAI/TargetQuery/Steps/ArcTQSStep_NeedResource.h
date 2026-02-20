// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "ArcTQSStep_NeedResource.generated.h"

/**
 * Scores items by combining the querier entity's current need level with
 * how much of a matching resource the target entity provides.
 *
 * The querier's need is read from a designer-selected FArcNeedFragment subclass.
 * The target's resource is read from a designer-selected FArcResourceFragment subclass.
 *
 * Final raw score = NeedFactor * ResourceFactor, where:
 *   NeedFactor     = QuerierNeed.CurrentValue / 100  (normalized 0-1)
 *   ResourceFactor = clamp(TargetResource.CurrentResourceAmount / MaxResourceForNormalization, 0, 1)
 *
 * This is then passed through the ResponseCurve.
 * A high need + abundant resource = high score.
 * Use an inverted response curve if you want to prefer targets when need is LOW.
 */
USTRUCT(DisplayName = "Need/Resource Score")
struct ARCAI_API FArcTQSStep_NeedResource : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_NeedResource()
	{
		StepType = EArcTQSStepType::Score;
	}

	// The need fragment type to read from the querier entity (must derive from FArcNeedFragment)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (MetaStruct = "/Script/ArcAI.ArcNeedFragment"))
	TObjectPtr<UScriptStruct> NeedType;

	// The resource fragment type to read from the target entity (must derive from FArcResourceFragment)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (MetaStruct = "/Script/ArcAI.ArcResourceFragment"))
	TObjectPtr<UScriptStruct> ResourceType;

	// Maximum resource amount for normalization. Resource values above this clamp to 1.0.
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.01))
	float MaxResourceForNormalization = 100.0f;

	// Score assigned to items when the querier has no need fragment or target has no resource
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float DefaultScore = 0.0f;

	// If true, items without a valid resource fragment are filtered out
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bFilterItemsWithoutResource = false;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
