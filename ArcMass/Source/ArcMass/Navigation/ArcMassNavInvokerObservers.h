// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "ArcMassNavInvokerObservers.generated.h"

/**
 * Observer that fires when FMassNavInvokerFragment is added to a moveable entity
 * (i.e. entities WITHOUT FMassNavInvokerStaticTag). Registers the invoker with
 * UArcMassNavInvokerSubsystem and stores the allocated slot index.
 */
UCLASS()
class ARCMASS_API UArcMassNavInvokerMoveableObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassNavInvokerMoveableObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/**
 * Observer that fires when FMassNavInvokerFragment is added to a static entity
 * (i.e. entities WITH FMassNavInvokerStaticTag). Registers the invoker once;
 * no per-frame movement tracking will be performed for static entities.
 */
UCLASS()
class ARCMASS_API UArcMassNavInvokerStaticObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassNavInvokerStaticObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

/**
 * Observer that fires when FMassNavInvokerFragment is removed from any entity.
 * Frees the subsystem slot and removes the entity's associated navmesh tiles.
 */
UCLASS()
class ARCMASS_API UArcMassNavInvokerRemovalObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcMassNavInvokerRemovalObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
