// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassObserverProcessor.h"
#include "ArcIWEntityDeinitObserver.generated.h"

// ---------------------------------------------------------------------------
// Entity Deinit Observer
// Observes FArcIWEntityTag Remove -- cleans up ISM instances, unregisters from grid.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWEntityDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcIWEntityDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
