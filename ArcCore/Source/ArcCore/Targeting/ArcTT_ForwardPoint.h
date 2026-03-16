// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScalableFloat.h"
#include "Tasks/TargetingTask.h"
#include "ArcTT_ForwardPoint.generated.h"

/**
 * Returns a point in space aligned to the source actor's forward direction.
 * No tracing is performed — the point is computed purely from actor orientation and offsets.
 */
UCLASS()
class ARCCORE_API UArcTT_ForwardPoint : public UTargetingTask
{
	GENERATED_BODY()

public:
	/** Forward distance from the actor */
	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat Distance = 500.f;

	/** Lateral offset (positive = right, negative = left) */
	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat RightOffset = 0.f;

	/** Vertical offset (positive = up, negative = down) */
	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat UpOffset = 0.f;

	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;

#if ENABLE_DRAW_DEBUG
	virtual void DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const override;
#endif
};
