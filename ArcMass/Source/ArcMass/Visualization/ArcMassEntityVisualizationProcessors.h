// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ObserverProcessor.h"
#include "Mass/Processor.h"
#include "MassSignalProcessorBase.h"
#include "ArcMassEntityVisualizationProcessors.generated.h"

// ---------------------------------------------------------------------------
// Player Cell Tracking Processor
// Runs every frame, detects player cell changes, signals affected entities
// using hysteresis (activation/deactivation radii).
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

	// Reusable per-crossing scratch — GT-only processor, not re-entrant
	// Mesh grid block
	TArray<FIntVector> ScratchNewActivationCells;
	TArray<FIntVector> ScratchNewDeactivationCells;
	TSet<FIntVector>   ScratchOldDeactivationSet;
	TSet<FIntVector>   ScratchNewDeactivationSet;
	TArray<FIntVector> ScratchOldActivationCells;
	TArray<FIntVector> ScratchOldDeactivationCells;
	TArray<FMassEntityHandle> ScratchDeactivatedEntities;
	TArray<FMassEntityHandle> ScratchActivatedEntities;
	// Physics grid block
	TArray<FIntVector> ScratchNewPhysActivationCells;
	TArray<FIntVector> ScratchNewPhysDeactivationCells;
	TSet<FIntVector>   ScratchOldPhysDeactivationSet;
	TSet<FIntVector>   ScratchNewPhysDeactivationSet;
	TArray<FIntVector> ScratchOldPhysActivationCells;
	TArray<FIntVector> ScratchOldPhysDeactivationCells;
	TArray<FMassEntityHandle> ScratchPhysDeactivatedEntities;
	TArray<FMassEntityHandle> ScratchPhysActivatedEntities;
};

// ---------------------------------------------------------------------------
// Mesh Activate Processor
// Subscribes to VisMeshActivated — finds or creates an ISM holder for the
// entity's cell/mesh config and adds the entity as an ISM instance.
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisMeshActivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisMeshActivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Mesh Deactivate Processor
// Subscribes to VisMeshDeactivated — removes the entity's ISM instance from
// its holder (destroying the holder if empty).
// ---------------------------------------------------------------------------

UCLASS()
class ARCMASS_API UArcVisMeshDeactivateProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcVisMeshDeactivateProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};

// ---------------------------------------------------------------------------
// Entity Init Observer
// Observes FArcVisEntityTag Add — registers entity in grid and signals
// VisMeshActivated if in an active cell.
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
// Observes FArcVisEntityTag Remove — removes ISM instance if active, cleans
// up actors, and unregisters from grid.
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
