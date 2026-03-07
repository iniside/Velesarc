// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Tasks/TargetingTask.h"
#include "ArcTT_ItemBoxTrace.generated.h"

UCLASS()
class ARCCORE_API UArcTT_ItemBoxTrace : public UTargetingTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};
