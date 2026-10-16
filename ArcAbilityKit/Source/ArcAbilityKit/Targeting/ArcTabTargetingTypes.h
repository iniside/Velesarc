// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcTabTargetingTypes.generated.h"

USTRUCT()
struct ARCABILITYKIT_API FArcTabTargetRetentionPolicy
{
	GENERATED_BODY()

	UPROPERTY()
	float LoseTargetRange = 0.0f;

	UPROPERTY()
	FGameplayTag DeathTag;

	UPROPERTY()
	FVector TargetLocationOffset = FVector::ZeroVector;
};
