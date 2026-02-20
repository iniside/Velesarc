// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "StructUtils/InstancedStruct.h"
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

	// If true, use LocationProvider to resolve path start. If false, use PathStart directly.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bUseLocationProvider = true;

	// Location provider for DataAsset definitions
	UPROPERTY(EditAnywhere, Category = "Step", meta = (BaseStruct = "/Script/ArcAI.ArcTQSLocationProvider", EditCondition = "bUseLocationProvider"))
	FInstancedStruct LocationProvider;

	// Direct path start for State Tree property binding
	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "!bUseLocationProvider"))
	FVector PathStart = FVector::ZeroVector;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
