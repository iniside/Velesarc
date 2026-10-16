// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ObserverProcessor.h"
#include "MassSignalProcessorBase.h"
#include "ArcCompositeMeshVisualizationProcessors.generated.h"

// ---------------------------------------------------------------------------
// Init Observer
// Observes FArcCompositeMeshVisEntityTag Add — registers entity in the mesh
// grid and signals VisMeshActivated if the entity is in an active cell.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcCompositeMeshVisEntityInitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcCompositeMeshVisEntityInitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};

// ---------------------------------------------------------------------------
// Activate Processor
// Subscribes to VisMeshActivated — for each sub-mesh entry: finds/creates an
// ISM holder and adds a per-entry ISM instance, storing the N {Holder,Index}
// pairs in FArcCompositeMeshVisInstanceLinkFragment.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcCompositeMeshVisActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcCompositeMeshVisActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Deactivate Processor
// Subscribes to VisMeshDeactivated — removes each sub-mesh ISM instance from
// its holder and resets the link fragment in place.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcCompositeMeshVisDeactivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcCompositeMeshVisDeactivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Deinit Observer
// Observes FArcCompositeMeshVisEntityTag Remove — removes any live ISM
// instances and unregisters the entity from the mesh grid.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcCompositeMeshVisEntityDeinitObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcCompositeMeshVisEntityDeinitObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
