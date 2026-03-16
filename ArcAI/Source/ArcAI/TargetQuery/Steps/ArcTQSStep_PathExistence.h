// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "TargetQuery/ArcTQSLocationProvider.h"
#include "ArcTQSStep_PathExistence.generated.h"

/**
 * Pathfinding existence test. Checks whether a navigation path exists from
 * a reference location to the item's location using TestPathSync.
 *
 * Typically used as a Filter to discard unreachable items.
 */
USTRUCT(DisplayName = "Path Existence Test")
struct ARCAI_API FArcTQSStep_PathExistence : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_PathExistence()
	{
		StepType = EArcTQSStepType::Filter;
	}

	/** Path start location configuration. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FArcTQSLocationConfig LocationConfig;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
