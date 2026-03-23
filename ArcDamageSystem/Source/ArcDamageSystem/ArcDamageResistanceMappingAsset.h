// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "ArcDamageResistanceMappingAsset.generated.h"

/**
 * Maps a DamageType.* gameplay tag to a condition fragment type.
 * Used by UArcDamageAttributeSet to sync DamageResistance into condition fragment State.Resistance.
 */
USTRUCT(BlueprintType)
struct ARCDAMAGESYSTEM_API FArcDamageConditionMapping
{
	GENERATED_BODY()

	/** DamageType.* tag used to filter the resistance aggregator. */
	UPROPERTY(EditAnywhere)
	FGameplayTag DamageTypeTag;

	/** Condition fragment type whose State.Resistance receives the filtered value. Must derive from FArcConditionFragment. */
	UPROPERTY(EditAnywhere, meta = (MetaStruct = "/Script/ArcConditionEffects.ArcConditionFragment"))
	TObjectPtr<UScriptStruct> ConditionFragmentType = nullptr;
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
