// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "ArcUtilityConsideration_LineOfSight.generated.h"

/**
 * Line-of-sight trace from querier to target.
 * Returns 1.0 if visible, 0.0 if blocked.
 */
USTRUCT(BlueprintType, DisplayName = "Line Of Sight Consideration")
struct ARCAI_API FArcUtilityConsideration_LineOfSight : public FArcUtilityConsideration
{
	GENERATED_BODY()

	/** Collision channel used for the trace. */
	UPROPERTY(EditAnywhere, Category = "Consideration")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Height offset added to both start and end trace points. */
	UPROPERTY(EditAnywhere, Category = "Consideration")
	float TraceHeightOffset = 50.0f;

	virtual float Score(const FArcUtilityTarget& Target, const FArcUtilityContext& Context) const override;
};
