// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassObserverProcessor.h"
#include "ArcIWSimpleVisEntityDeinitObserver.generated.h"

UCLASS()
class ARCINSTANCEDWORLD_API UArcIWSimpleVisEntityDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcIWSimpleVisEntityDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
	FMassEntityQuery SkinnedObserverQuery;
};
