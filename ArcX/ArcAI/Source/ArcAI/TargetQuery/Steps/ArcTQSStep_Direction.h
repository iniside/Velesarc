// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TargetQuery/ArcTQSStep.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTQSStep_Direction.generated.h"

/**
 * Scores or filters items based on directional alignment.
 *
 * Computes how well the direction from the querier to each item aligns with
 * the direction from the querier to a reference location. Uses dot product
 * mapped from [-1, 1] to [0, 1].
 *
 * Reference location can come from:
 * - LocationProvider (instanced struct, for DataAsset definitions)
 * - ReferenceLocation (FVector, for State Tree inline definitions via property binding)
 * Set bUseLocationProvider to choose which source to use.
 */
USTRUCT(DisplayName = "Direction Score")
struct ARCAI_API FArcTQSStep_Direction : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_Direction()
	{
		StepType = EArcTQSStepType::Score;
	}

	// If true, use LocationProvider to resolve reference. If false, use ReferenceLocation directly.
	UPROPERTY(EditAnywhere, Category = "Step")
	bool bUseLocationProvider = true;

	/**
	 * Location provider that resolves the reference point at runtime.
	 * Used in DataAsset definitions where property binding is not available.
	 */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (BaseStruct = "/Script/ArcAI.ArcTQSLocationProvider", EditCondition = "bUseLocationProvider"))
	FInstancedStruct LocationProvider;

	/**
	 * Direct reference location. Use property binding in State Tree to set this at runtime.
	 * Only used when bUseLocationProvider is false.
	 */
	UPROPERTY(EditAnywhere, Category = "Step", meta = (EditCondition = "!bUseLocationProvider"))
	FVector ReferenceLocation = FVector::ZeroVector;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
