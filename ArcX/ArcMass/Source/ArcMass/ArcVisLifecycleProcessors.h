// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassProcessor.h"
#include "MassSignalProcessorBase.h"
#include "ArcVisLifecycleProcessors.generated.h"

// ---------------------------------------------------------------------------
// Init Observer — sets initial phase on FArcVisLifecycleTag add
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisLifecycleInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcVisLifecycleInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// Tick Processor — ticks phase timers, triggers natural transitions
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisLifecycleTickProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcVisLifecycleTickProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};

// ---------------------------------------------------------------------------
// Force Transition Processor — handles external forced transitions
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisLifecycleForceTransitionProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisLifecycleForceTransitionProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Vis Switch Processor — bridges lifecycle phase changes to visual changes.
// For ISM entities: defers mesh switch fragment.
// For Actor entities: swaps actor or notifies interface.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisLifecycleVisSwitchProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisLifecycleVisSwitchProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// ISM Mesh Switch Processor — processes deferred FArcVisLifecycleMeshSwitchFragment.
// Removes old ISM instance, adds new one with resolved phase mesh.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisLifecycleISMMeshSwitchProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcVisLifecycleISMMeshSwitchProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery EntityQuery;
};
