// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/ObserverProcessor.h"

#include "ArcTacticsPositionQueryObserver.generated.h"

UCLASS()
class ARCTACTICS_API UArcTacticsPositionQueryObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcTacticsPositionQueryObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery{*this};
};
