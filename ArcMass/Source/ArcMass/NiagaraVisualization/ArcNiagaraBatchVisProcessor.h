// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcNiagaraBatchVisProcessor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/**
 * Batch Niagara visualization processor for Mass entities.
 *
 * Creates ONE UNiagaraComponent per unique Niagara system asset and feeds
 * all matching entity positions into a Position Array Data Interface each frame.
 * The Niagara system reads positions from the array to render all entities.
 *
 * This is the Data Interface alternative to the per-entity approach
 * (UArcNiagaraVisDynamicDataProcessor + FArcNiagaraRenderStateHelper).
 *
 * Batch states are lazily created on first encounter and cleaned up when
 * the processor is deinitialized (world teardown).
 */
UCLASS()
class ARCMASS_API UArcNiagaraBatchVisProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcNiagaraBatchVisProcessor();

	virtual void BeginDestroy() override;

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	/** Per-batch runtime state — one per unique Niagara system asset. */
	struct FBatchState
	{
		/** Actor that hosts the Niagara component (spawned at origin, transient). */
		TWeakObjectPtr<AActor> OwnerActor;

		/** The Niagara component that ticks and renders the batch system. */
		TWeakObjectPtr<UNiagaraComponent> NiagaraComponent;

		/** Name of the Position Array parameter on the Niagara system. */
		FName PositionParamName;

		/** Reusable position buffer — Reset() each frame, keeps allocation. */
		TArray<FVector> PositionBuffer;
	};

	/** Get or lazily create a batch state for the given system asset. */
	FBatchState& GetOrCreateBatch(UWorld& World, UNiagaraSystem& System, FName PositionParamName);

	FMassEntityQuery EntityQuery;

	/** Map from Niagara system asset → batch state. */
	TMap<TObjectKey<UNiagaraSystem>, FBatchState> BatchStates;
};
