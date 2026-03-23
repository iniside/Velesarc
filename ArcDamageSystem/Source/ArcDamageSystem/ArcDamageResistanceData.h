// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArcAbilitySystemData.h"
#include "ArcDamageResistanceData.generated.h"

class UArcDamageResistanceMappingAsset;

USTRUCT(BlueprintType)
struct ARCDAMAGESYSTEM_API FArcDamageResistanceData : public FArcAbilitySystemData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<UArcDamageResistanceMappingAsset> ResistanceMappingAsset;
};
