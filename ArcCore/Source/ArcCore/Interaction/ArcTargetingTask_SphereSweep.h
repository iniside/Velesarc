// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ScalableFloat.h"
#include "Tasks/TargetingTask.h"
#include "ArcTargetingTask_SphereSweep.generated.h"

UCLASS()
class ARCCORE_API UArcTargetingTask_SphereSweep : public UTargetingTask
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Config")
	TEnumAsByte<ETraceTypeQuery> TraceChannel = ETraceTypeQuery::TraceTypeQuery1;

	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat Radius = 700.f;

	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};
