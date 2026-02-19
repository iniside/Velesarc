// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassEntityVisualizationProcessors.generated.h"

// ---------------------------------------------------------------------------
// Player Cell Tracking Processor
// Runs every frame, detects player cell changes, signals affected entities.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisPlayerCellTrackingProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcVisPlayerCellTrackingProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

// ---------------------------------------------------------------------------
// Swap ISM → Actor on cell activation signal.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Swap Actor → ISM on cell deactivation signal.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisDeactivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisDeactivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Entity Init Observer
// Observes FArcVisEntityTag Add — registers entity in grid, creates ISM instance.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisEntityInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcVisEntityInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// Entity Deinit Observer
// Observes FArcVisEntityTag Remove — cleans up actor or ISM, unregisters from grid.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisEntityDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcVisEntityDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
