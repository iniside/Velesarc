// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSLocationProvider.h"
#include "ArcTQSStep_Trace.generated.h"

/**
 * Visibility/line trace test from reference locations to each item.
 * Configure LocationConfig to set trace origin sources.
 *
 * As a Filter: items not visible from the trace origin are discarded.
 * As a Score: items get 1.0 if visible, 0.0 if not (apply response curve for softer results).
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

	/** Trace origin location configuration. */
	UPROPERTY(EditAnywhere, Category = "Step")
	FArcTQSLocationConfig LocationConfig;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
