// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_Angle.generated.h"

/**
 * Scores based on the angle between the querier's forward direction and the
 * direction to the target. 0 degrees (directly ahead) = 1.0, MaxAngle = 0.0.
 * Targets beyond MaxAngle score 0.
 */
USTRUCT(BlueprintType, DisplayName = "Angle Consideration")
struct ARCAI_API FArcUtilityConsideration_Angle : public FArcUtilityConsideration
{
	GENERATED_BODY()

	/** Maximum angle (degrees) at which score reaches 0. */
	UPROPERTY(EditAnywhere, Category = "Consideration", meta = (ClampMin = 1.0, ClampMax = 180.0))
	float MaxAngleDegrees = 180.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
