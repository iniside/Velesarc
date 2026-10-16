// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ArcTierTypes.generated.h"

class UArcEffectDefinition;

USTRUCT(BlueprintType, meta = (ToolTip = "Ascending thresholds. The tier of a magnitude is the 1-based index of the highest threshold it reaches; below the first threshold the tier is 0."))
struct ARCMASSMECHANICS_API FArcTierBands
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tiers")
	TArray<float> Thresholds;

	int32 GetTier(float Magnitude) const;
};

USTRUCT(BlueprintType)
struct ARCMASSMECHANICS_API FArcTierEffectEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tier")
	FGameplayTag Key;

	UPROPERTY(EditAnywhere, Category = "Tier")
	int32 Tier = 0;

	UPROPERTY(EditAnywhere, Category = "Tier", meta = (ToolTip = "The effect for this (Key, Tier). The Evaluate Threshold Tier task applies it once and keeps no handle, so Instant effects are allowed."))
	TObjectPtr<UArcEffectDefinition> Effect = nullptr;
};

UCLASS(BlueprintType, meta = (ToolTip = "Maps a (Key, Tier) pair to an effect for that tier. The Evaluate Threshold Tier task applies it once."))
class ARCMASSMECHANICS_API UArcTierEffectTable : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Tiers")
	TArray<FArcTierEffectEntry> Entries;

	UArcEffectDefinition* FindEffect(const FGameplayTag& Key, int32 Tier) const;
};
