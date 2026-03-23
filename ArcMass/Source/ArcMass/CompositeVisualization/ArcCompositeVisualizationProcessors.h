// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassSignalProcessorBase.h"
#include "ArcCompositeVisualizationProcessors.generated.h"

// ---------------------------------------------------------------------------
// UArcCompositeVisEntityInitObserver
// ---------------------------------------------------------------------------

/** Observes FArcCompositeVisEntityTag Add — registers entity and optionally spawns actor. */
UCLASS()
class ARCMASS_API UArcCompositeVisEntityInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcCompositeVisEntityInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// UArcCompositeVisActivateProcessor
// ---------------------------------------------------------------------------

/** Signal processor — transitions entities from ISM to Actor on cell activation. */
UCLASS()
class ARCMASS_API UArcCompositeVisActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcCompositeVisActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// UArcCompositeVisDeactivateProcessor
// ---------------------------------------------------------------------------

/** Signal processor — transitions entities from Actor to ISM on cell deactivation. */
UCLASS()
class ARCMASS_API UArcCompositeVisDeactivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcCompositeVisDeactivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// UArcCompositeVisEntityDeinitObserver
// ---------------------------------------------------------------------------

/** Observes FArcCompositeVisEntityTag Remove — cleans up actor or ISM instances and unregisters entity. */
UCLASS()
class ARCMASS_API UArcCompositeVisEntityDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcCompositeVisEntityDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
