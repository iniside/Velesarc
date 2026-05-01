// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"

#include "ArcMassDamageResistanceMappingAsset.generated.h"

USTRUCT(BlueprintType)
struct ARCMASSDAMAGESYSTEM_API FArcMassDamageConditionMapping
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "12"))
	int32 ConditionTypeIndex = 0;
};

UCLASS(BlueprintType)
class ARCMASSDAMAGESYSTEM_API UArcMassDamageResistanceMappingAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<FArcMassDamageConditionMapping> Mappings;
};
