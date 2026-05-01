// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "ArcDamageResistanceMappingAsset.generated.h"

/**
 * Maps a DamageType.* gameplay tag to a condition type index.
 * Used by UArcDamageAttributeSet to sync DamageResistance into condition State.Resistance.
 */
USTRUCT(BlueprintType)
struct ARCDAMAGESYSTEM_API FArcDamageConditionMapping
{
	GENERATED_BODY()

	/** DamageType.* tag used to filter the resistance aggregator. */
	UPROPERTY(EditAnywhere)
	FGameplayTag DamageTypeTag;

	/** Index into FArcConditionStatesFragment::States (cast of EArcConditionType). */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "12"))
	int32 ConditionTypeIndex = 0;
};

/**
 * Data asset defining which damage type tags map to which condition fragments
 * for the resistance bridge in UArcDamageAttributeSet.
 */
UCLASS()
class ARCDAMAGESYSTEM_API UArcDamageResistanceMappingAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly)
	TArray<FArcDamageConditionMapping> Mappings;
};
