// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Faction/ArcFactionTypes.h"
#include "ArcArchetypeRewardWeights.generated.h"

/** Per-channel reward weights for a single archetype. */
USTRUCT(BlueprintType)
struct ARCECONOMY_API FArcRewardChannelWeights
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Reward", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Economic = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Reward", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Military = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Reward", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Diplomatic = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Reward", meta = (ClampMin = "0.0"))
    float SurvivalPenalty = 10.0f;
};

/**
 * Maps faction archetypes to reward channel weights.
 * Used by the training subsystem to shape agent behavior per archetype.
 */
UCLASS(BlueprintType)
class ARCECONOMY_API UArcArchetypeRewardWeights : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Reward")
    TMap<EArcFactionArchetype, FArcRewardChannelWeights> ArchetypeWeights;

    FArcRewardChannelWeights GetWeights(EArcFactionArchetype Archetype) const
    {
        const FArcRewardChannelWeights* Found = ArchetypeWeights.Find(Archetype);
        return Found ? *Found : FArcRewardChannelWeights();
    }
};
