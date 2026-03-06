// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ScalableFloat.h"
#include "Tasks/TargetingSortTask_Base.h"
#include "ArcTargetingSortTask_InteractionScore.generated.h"

UCLASS(Blueprintable)
class ARCCORE_API UArcTargetingSortTask_InteractionScore : public UTargetingSortTask_Base
{
	GENERATED_BODY()

protected:
	/** Weight for distance factor (closer = better score) */
	UPROPERTY(EditAnywhere, Category = "Config")
	float DistanceWeight = 0.5f;

	/** Weight for look-direction alignment (more aligned with view = better score) */
	UPROPERTY(EditAnywhere, Category = "Config")
	float LookDirectionWeight = 0.5f;

	/** Maximum distance for normalization. Targets beyond this get worst distance score. */
	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat MaxDistance = 1000.f;

	virtual float GetScoreForTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const override;
};
