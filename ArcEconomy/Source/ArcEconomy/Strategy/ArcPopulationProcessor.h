// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcPopulationProcessor.generated.h"

/**
 * Ticks population growth and decline for settlements.
 *
 * Growth: food surplus + housing capacity + stability.
 * Decline: combat deaths (external), starvation, abandonment.
 */
UCLASS()
class ARCECONOMY_API UArcPopulationProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UArcPopulationProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery SettlementQuery;

    /** Minimum Security value (0–100) for growth to occur. */
    float MinSecurityForGrowth = 30.0f;
};
