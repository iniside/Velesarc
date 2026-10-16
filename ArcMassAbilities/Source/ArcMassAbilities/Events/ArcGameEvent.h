// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcGameEvent.generated.h"

UENUM()
enum class EArcEventRole : uint8
{
	Instigator,
	Target,
	Any
};

USTRUCT()
struct ARCMASSABILITIES_API FArcGameEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	FMassEntityHandle Instigator;

	UPROPERTY()
	FMassEntityHandle Target;

	UPROPERTY()
	float Magnitude = 0.f;

	UPROPERTY()
	FGameplayTagContainer SourceTags;

	UPROPERTY()
	FGameplayTagContainer TargetTags;

	UPROPERTY()
	FInstancedStruct Payload;
};
