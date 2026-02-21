// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTQSStep_DistanceFilter.generated.h"

/**
 * Hard distance filter. Discards items outside the specified range from a reference location.
 *
 * When the location provider returns multiple locations, the item passes if it is
 * within range of ANY of the returned locations.
 */
USTRUCT(DisplayName = "Distance Filter")
struct ARCAI_API FArcTQSStep_DistanceFilter : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_DistanceFilter()
	{
		StepType = EArcTQSStepType::Filter;
	}

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0))
	float MinDistance = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 0.0))
	float MaxDistance = 5000.0f;

	// If true, use LocationProvider to resolve reference location. If false, use ReferenceLocation directly.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bUseLocationProvider = true;

	// Location provider for DataAsset definitions (can return multiple locations)
	UPROPERTY(EditAnywhere, Category = "Step", meta = (BaseStruct = "/Script/ArcAI.ArcTQSLocationProvider", EditCondition = "bUseLocationProvider"))
	FInstancedStruct LocationProvider;

	// Direct reference location for State Tree property binding
	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "!bUseLocationProvider"))
	FVector ReferenceLocation = FVector::ZeroVector;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
