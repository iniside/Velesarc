// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "ArcGovernorProcessor.generated.h"

/**
 * Runs all AI governor decision-making for settlements with FArcGovernorFragment.
 * Replaces the 5 governor StateTree tasks with a single processor.
 * All cross-entity mutations are batched into deferred commands.
 */
UCLASS()
class ARCECONOMY_API UArcGovernorProcessor : public UMassProcessor
{
    GENERATED_BODY()

public:
    UArcGovernorProcessor();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery GovernorQuery;
};
