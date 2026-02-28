// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcSettlementObservers.generated.h"

/**
 * Observer that fires when FArcSettlementMemberTag is added to an entity.
 * Registers the entity's capabilities as knowledge entries in its settlement.
 */
UCLASS()
class ARCSETTLEMENT_API UArcSettlementMemberAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcSettlementMemberAddObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/**
 * Observer that fires when FArcSettlementMemberTag is removed from an entity.
 * Removes the entity's registered knowledge entries from its settlement.
 */
UCLASS()
class ARCSETTLEMENT_API UArcSettlementMemberRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcSettlementMemberRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
