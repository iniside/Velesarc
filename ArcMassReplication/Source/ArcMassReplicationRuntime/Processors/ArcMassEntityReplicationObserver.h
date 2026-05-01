// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassEntityQuery.h"
#include "ArcMassEntityReplicationObserver.generated.h"

UCLASS()
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityReplicationStartObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassEntityReplicationStartObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

UCLASS()
class ARCMASSREPLICATIONRUNTIME_API UArcMassEntityReplicationStopObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassEntityReplicationStopObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
