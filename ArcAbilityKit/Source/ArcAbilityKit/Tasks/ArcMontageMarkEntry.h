// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMontageMarkEntry.generated.h"

/**
 * A single time-keyed mark on a montage-with-marks task.
 * The companion processor counts crossings; the task broadcasts Dispatcher
 * exactly once each time the playhead crosses Time (looping fires once per cycle).
 */
USTRUCT(BlueprintType)
struct ARCABILITYKIT_API FArcMontageMarkEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mark", meta = (ClampMin = "0.0"))
	float Time = 0.f;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid SourceNotifyGuid;
#endif

	UPROPERTY(EditAnywhere, Category = "Mark")
	FStateTreeDelegateDispatcher Dispatcher;

	UPROPERTY(EditAnywhere, Category = "Mark")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = "Mark")
	FInstancedStruct Payload;

	UPROPERTY(EditAnywhere, Category = "Mark")
	FName DebugLabel;
};
