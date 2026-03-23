// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassObserverProcessor.h"
#include "ArcIWEntityInitObserver.generated.h"

// ---------------------------------------------------------------------------
// Entity Init Observer
// Observes FArcIWEntityTag Add -- registers entity in grid, creates ISM instances.
// ---------------------------------------------------------------------------

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWEntityInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcIWEntityInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
