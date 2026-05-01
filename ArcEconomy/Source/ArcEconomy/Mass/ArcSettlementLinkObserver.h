// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcSettlementLinkObserver.generated.h"

/**
 * Observer that resolves a settlement's FArcEntityRef links when FArcSettlementFragment is added.
 * Runs after the ArcLinkableGuidInit group so all GUIDs are already registered in
 * UArcLinkableGuidSubsystem.
 */
UCLASS()
class ARCECONOMY_API UArcSettlementLinkObserver : public UMassObserverProcessor
{
    GENERATED_BODY()

public:
    UArcSettlementLinkObserver();

protected:
    virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
    virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
    FMassEntityQuery ObserverQuery;
};
