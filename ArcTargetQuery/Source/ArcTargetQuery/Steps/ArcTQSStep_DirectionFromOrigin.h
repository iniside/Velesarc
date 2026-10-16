// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSStep.h"
#include "ArcTQSStep_DirectionFromOrigin.generated.h"

/**
 * Scores or filters items based on alignment with OriginEntity's facing.
 *
 * Reads OriginEntity's forward vector and scores Dot(OriginForward, OriginToItem),
 * where OriginToItem points from Origin to the item, mapped from [-1, 1] to [0, 1]; the
 * pipeline applies ResponseCurve. A curve peaking at 0 means "behind the origin entity"; at 1
 * means "in front of it" — this is what makes attacking from behind authorable.
 */
USTRUCT(DisplayName = "Direction From Origin")
struct ARCTARGETQUERY_API FArcTQSStep_DirectionFromOrigin : public FArcTQSStep
{
	GENERATED_BODY()

	FArcTQSStep_DirectionFromOrigin()
	{
		StepType = EArcTQSStepType::Score;
	}

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Step", meta = (ArcTQSRequiredBinding))
	FMassEntityHandle OriginEntity;

	virtual float ExecuteStep(const FArcTQSTargetItem& Item, const FArcTQSQueryContext& QueryContext) const override;
};
