// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTQSStep_Distance.generated.h"

/**
 * Distance-based scoring step.
 * Produces a 0-1 normalized score based on distance from a reference location to the item.
 *
 * When the location provider returns multiple locations, the item is scored against
 * each one and the best (highest) score is used.
 */
USTRUCT(DisplayName = "Distance Score")
struct ARCAI_API FArcTQSStep_Distance : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Distance()
	{
		StepType = EArcTQSStepType::Score;
	}

	// Maximum distance for normalization. Items beyond this get score 0 (if bPreferCloser) or 1.
	UPROPERTY(EditAnywhere, Category = "Step", meta = (ClampMin = 1.0))
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
