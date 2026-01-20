// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ArcCoreAsyncMessageTypes.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcCoreGenericTagMessage
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Tag;
};
