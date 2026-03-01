// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcMassAsyncMessageEndpointObservers.generated.h"

/**
 * Observer that creates an async message endpoint when
 * FArcMassAsyncMessageEndpointTag is added to an entity.
 */
UCLASS()
class ARCMASS_API UArcMassAsyncMessageEndpointAddObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassAsyncMessageEndpointAddObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/**
 * Observer that cleans up the async message endpoint when
 * FArcMassAsyncMessageEndpointTag is removed from an entity.
 */
UCLASS()
class ARCMASS_API UArcMassAsyncMessageEndpointRemoveObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassAsyncMessageEndpointRemoveObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
