// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcHitTypes.generated.h"

class UArcEffectDefinition;

UENUM(BlueprintType)
enum class EArcHitOutcome : uint8
{
	Hit UMETA(Hidden),
	Miss,
	Dodge,
	Parry,
	Block
};

USTRUCT(BlueprintType)
struct ARCMASSMECHANICS_API FArcHitResolvedPayload
{
	GENERATED_BODY()

	UPROPERTY()
	EArcHitOutcome Outcome = EArcHitOutcome::Hit;

	UPROPERTY()
	TWeakObjectPtr<UArcEffectDefinition> Definition;

	UPROPERTY()
	bool bCrit = false;

	UPROPERTY()
	FGameplayTag Zone;

	UPROPERTY()
	FGameplayTag DamageType;

	UPROPERTY()
	float Damage = 0.f;
};

USTRUCT(BlueprintType)
struct ARCMASSMECHANICS_API FArcDamageDealtPayload
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UArcEffectDefinition> Definition;

	UPROPERTY()
	FGameplayTag Zone;

	UPROPERTY()
	bool bCrit = false;

	UPROPERTY()
	bool bPeriodicTick = false;

	UPROPERTY()
	FGameplayTag DamageType;

	UPROPERTY()
	float PreMitigation = 0.f;

	UPROPERTY()
	float Final = 0.f;
};
