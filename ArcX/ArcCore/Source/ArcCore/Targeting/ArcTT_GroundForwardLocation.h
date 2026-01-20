// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ScalableFloat.h"
#include "Tasks/TargetingTask.h"
#include "ArcTT_GroundForwardLocation.generated.h"

/**
 * 
 */
UCLASS()
class ARCCORE_API UArcTT_GroundForwardLocation : public UTargetingTask
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Config")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;
	
	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat Distance = 500.f;
	
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};

UCLASS()
class ARCCORE_API UArcTT_GroundForwardBox : public UTargetingTask
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Config")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;
	
	UPROPERTY(EditAnywhere, Category = "Config")
	FScalableFloat Distance = 500.f;
	
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};