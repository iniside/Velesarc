// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Tasks/TargetingTask.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcTT_ItemSphereTrace.generated.h"

UCLASS()
class ARCCORE_API UArcTT_ItemSphereTrace : public UTargetingTask
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Config")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (BaseStruct = "/Script/ArcCore.ArcTraceOrigin", ExcludeBaseStruct))
	FInstancedStruct TraceOriginOverride;

	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;

#if ENABLE_DRAW_DEBUG
	virtual void DrawDebug(UTargetingSubsystem* TargetingSubsystem, FTargetingDebugInfo& Info, const FTargetingRequestHandle& TargetingHandle, float XOffset, float YOffset, int32 MinTextRowsToAdvance) const override;
#endif
};
