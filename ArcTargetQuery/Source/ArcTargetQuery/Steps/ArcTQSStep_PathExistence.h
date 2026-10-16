// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_PathExistence.generated.h"

/**
 * Pathfinding existence test. Checks whether a navigation path exists from
 * a reference location to the item's location using TestPathSync.
 *
 * Typically used as a Filter to discard unreachable items.
 */
USTRUCT(DisplayName = "Path Existence Test")
struct ARCTARGETQUERY_API FArcTQSStep_PathExistence : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_PathExistence()
	{
		StepType = EArcTQSStepType::Filter;
	}

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FVector ReferenceLocation = FVector::ZeroVector;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
	virtual EArcTQSWorldRequirements GetWorldRequirements() const override { return EArcTQSWorldRequirements::Navigation; }
};
