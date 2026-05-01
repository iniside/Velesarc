// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "ArcSettlementMarketProcessor.generated.h"

/**
 * Reads supply/demand counters, adjusts prices, resets counters.
 * Updates SettlementMarket knowledge entry.
 * Runs every ~5 seconds.
 */
UCLASS()
class ARCECONOMY_API UArcSettlementMarketProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UArcSettlementMarketProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery SettlementQuery;
    float TimeSinceLastTick = 0.0f;
    float TickInterval = 5.0f;
};
