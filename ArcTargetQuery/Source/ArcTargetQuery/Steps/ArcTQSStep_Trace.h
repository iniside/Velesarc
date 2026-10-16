// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_Trace.generated.h"

/**
 * Visibility/line trace test from ReferenceLocation to each item.
 *
 * As a Filter: items not visible from ReferenceLocation are discarded.
 * As a Score: raw value is 1.0 if visible, 0.0 if not.
 */
USTRUCT(DisplayName = "Trace Test")
struct ARCTARGETQUERY_API FArcTQSStep_Trace : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Trace()
	{
		StepType = EArcTQSStepType::Filter;
	}

	// Trace channel to use
	UPROPERTY(EditAnywhere, Category = "Step")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	// Height offset added to trace start and end points
	UPROPERTY(EditAnywhere, Category = "Step")
	float TraceHeightOffset = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FVector ReferenceLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Step")
	TWeakObjectPtr<AActor> IgnoredActor;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
	virtual EArcTQSWorldRequirements GetWorldRequirements() const override { return EArcTQSWorldRequirements::Collision; }
};
