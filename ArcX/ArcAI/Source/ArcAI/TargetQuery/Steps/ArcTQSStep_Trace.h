// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTQSStep_Trace.generated.h"

/**
 * Visibility/line trace test from a reference location to each item.
 * By default traces from the item's context location. Can use a LocationProvider
 * for DataAsset definitions or a direct TraceOrigin for State Tree property binding.
 *
 * As a Filter: items not visible from the trace origin are discarded.
 * As a Score: items get 1.0 if visible, 0.0 if not (apply response curve for softer results).
 */
USTRUCT(DisplayName = "Trace Test")
struct ARCAI_API FArcTQSStep_Trace : public FArcTQSStep
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

	// If true, use LocationProvider to resolve trace origin. If false, use TraceOrigin directly.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bUseLocationProvider = true;

	// Location provider for DataAsset definitions (resolves trace origin per item)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (BaseStruct = "/Script/ArcAI.ArcTQSLocationProvider", EditCondition = "bUseLocationProvider"))
	FInstancedStruct LocationProvider;

	// Direct trace origin for State Tree property binding
	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "!bUseLocationProvider"))
	FVector TraceOrigin = FVector::ZeroVector;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
