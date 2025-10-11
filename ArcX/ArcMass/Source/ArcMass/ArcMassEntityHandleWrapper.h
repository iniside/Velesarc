// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "UObject/Object.h"
#include "ArcMassEntityHandleWrapper.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassEntityHandleWrapper
{
	GENERATED_BODY()
	
public:
	UPROPERTY()
	FMassEntityHandle EntityHandle;
};
