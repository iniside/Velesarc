// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Faction/ArcFactionTypes.h"
#include "ArcStrategyCurriculumAsset.generated.h"

/** A single curriculum stage for progressive training. */
USTRUCT(BlueprintType)
struct ARCECONOMY_API FArcCurriculumStage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Curriculum")
    FName StageName;

    UPROPERTY(EditAnywhere, Category = "Curriculum", meta = (ClampMin = "1"))
    int32 NumFactions = 2;

    UPROPERTY(EditAnywhere, Category = "Curriculum", meta = (ClampMin = "1"))
    int32 MinSettlementsPerFaction = 2;

    UPROPERTY(EditAnywhere, Category = "Curriculum", meta = (ClampMin = "1"))
    int32 MaxSettlementsPerFaction = 3;

    UPROPERTY(EditAnywhere, Category = "Curriculum")
    TArray<EArcFactionArchetype> AllowedArchetypes;

    UPROPERTY(EditAnywhere, Category = "Curriculum")
    bool bEnableMilitary = false;

    UPROPERTY(EditAnywhere, Category = "Curriculum")
    bool bEnableDiplomacy = false;

    UPROPERTY(EditAnywhere, Category = "Curriculum", meta = (ClampMin = "50"))
    int32 MaxEpisodeSteps = 200;

    UPROPERTY(EditAnywhere, Category = "Curriculum")
    float AdvanceRewardThreshold = 0.5f;

    UPROPERTY(EditAnywhere, Category = "Curriculum", meta = (ClampMin = "1"))
    int32 AdvanceEpisodeCount = 50;

    UPROPERTY(EditAnywhere, Category = "Curriculum")
    float WorldRadius = 10000.0f;

    UPROPERTY(EditAnywhere, Category = "Curriculum", meta = (ClampMin = "0.0"))
    FVector2f StartingResourceRange = FVector2f(20.0f, 80.0f);

    UPROPERTY(EditAnywhere, Category = "Curriculum", meta = (ClampMin = "1.0"))
    FVector2f StartingPopulationRange = FVector2f(10.0f, 50.0f);
};

/**
 * Ordered list of curriculum stages for progressive training.
 */
UCLASS(BlueprintType)
class ARCECONOMY_API UArcStrategyCurriculumAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Curriculum")
    TArray<FArcCurriculumStage> Stages;
};
