// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcPopulationFragment.generated.h"

/**
 * Tracks population count and growth state for a settlement.
 * Added to settlement entities alongside existing settlement fragments.
 */
USTRUCT()
struct ARCECONOMY_API FArcPopulationFragment : public FMassFragment
{
    GENERATED_BODY()

    /** Current population count. */
    UPROPERTY()
    int32 Population = 0;

    /** Maximum population based on housing capacity. */
    UPROPERTY(EditAnywhere, Category = "Population")
    int32 MaxPopulation = 20;

    /** Seconds between population growth ticks when conditions are met. */
    UPROPERTY(EditAnywhere, Category = "Population")
    float GrowthIntervalSeconds = 30.0f;

    /** Seconds of sustained food deficit before starvation deaths begin. */
    UPROPERTY(EditAnywhere, Category = "Population")
    float StarvationGracePeriod = 60.0f;

    /** Runtime: accumulated food deficit time. */
    UPROPERTY()
    float StarvationTimer = 0.0f;

    /** Runtime: time since last growth event. */
    UPROPERTY()
    float GrowthTimer = 0.0f;
};
