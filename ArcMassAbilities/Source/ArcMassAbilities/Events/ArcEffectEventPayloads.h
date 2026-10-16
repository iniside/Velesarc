// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/SharedStruct.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcEffectEventPayloads.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectRemovedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FSharedStruct Spec;

	UPROPERTY()
	FArcActiveEffectHandle Handle;

	UPROPERTY()
	EArcEffectRemovalReason Reason = EArcEffectRemovalReason::Removed;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectAppliedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FSharedStruct Spec;

	UPROPERTY()
	FArcActiveEffectHandle Handle;

	UPROPERTY()
	int32 StackCount = 0;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectExecutedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FSharedStruct Spec;

	UPROPERTY()
	FArcActiveEffectHandle Handle;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcEffectRejectedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FSharedStruct Spec;

	UPROPERTY()
	FGameplayTag Reason;
};
