// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcTagEventPayloads.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcTagChangedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	int32 OldCount = 0;

	UPROPERTY()
	int32 NewCount = 0;
};
