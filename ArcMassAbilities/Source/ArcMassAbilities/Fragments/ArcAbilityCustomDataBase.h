// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcAbilityCustomDataBase.generated.h"

/**
 * Base struct for typed ability custom data payloads stored in FArcAbilityCustomData.
 *
 * Subclass to carry per-source context (item, condition-effect, knowledge, etc.)
 * into an active ability. The editor dropdown on FArcAbilityCustomData UPROPERTY arrays
 * is filtered to FArcAbilityCustomDataBase subclasses via:
 *   meta=(BaseStruct="/Script/ArcMassAbilities.ArcAbilityCustomDataBase")
 */
USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcAbilityCustomDataBase
{
	GENERATED_BODY()

	virtual ~FArcAbilityCustomDataBase() = default;
};
