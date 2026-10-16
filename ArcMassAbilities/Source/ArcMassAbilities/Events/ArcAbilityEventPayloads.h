// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "ArcAbilityEventPayloads.generated.h"

class UArcAbilityDefinition;

UENUM()
enum class EArcAbilityEndReason : uint8
{
	Completed = 0,
	Cancelled = 1,
	Failed = 2,
	Interrupted = 3
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityEndedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<const UArcAbilityDefinition> Definition;

	UPROPERTY()
	FArcAbilityHandle Handle;

	UPROPERTY()
	EArcAbilityEndReason Reason = EArcAbilityEndReason::Completed;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityActivatedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<const UArcAbilityDefinition> Definition;

	UPROPERTY()
	FArcAbilityHandle Handle;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityCostFailedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<const UArcAbilityDefinition> Definition;

	UPROPERTY()
	EArcCostFailReason Reason = EArcCostFailReason::Insufficient;

	UPROPERTY()
	float MissingAmount = 0.f;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityCooldownFailedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<const UArcAbilityDefinition> Definition;

	UPROPERTY()
	float RemainingSeconds = 0.f;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityCooldownExpiredPayload
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<const UArcAbilityDefinition> Definition;

	UPROPERTY()
	FArcAbilityHandle Handle;
};
