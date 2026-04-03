// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcNiagaraBatchVisProcessor.h"

#include "ArcNiagaraBatchVisFragments.h"
#include "Mass/EntityFragments.h"
#include "MassExecutionContext.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

// ---------------------------------------------------------------------------
// UArcNiagaraBatchVisProcessor
// ---------------------------------------------------------------------------

UArcNiagaraBatchVisProcessor::UArcNiagaraBatchVisProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcNiagaraBatchVisProcessor::BeginDestroy()
{
	// Destroy all batch actors and their Niagara components
	for (auto& [Key, State] : BatchStates)
	{
		if (AActor* Actor = State.OwnerActor.Get())
		{
			Actor->Destroy();
		}
	}
	BatchStates.Empty();

	Super::BeginDestroy();
}

void UArcNiagaraBatchVisProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcNiagaraBatchVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcNiagaraBatchVisTag>(EMassFragmentPresence::All);
}

void UArcNiagaraBatchVisProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNiagaraBatchVis);

	// -----------------------------------------------------------------------
	// Phase 1: Clear all position buffers (Reset keeps allocation)
	// -----------------------------------------------------------------------
	for (auto& [Key, State] : BatchStates)
	{
		State.PositionBuffer.Reset();
	}

	// -----------------------------------------------------------------------
	// Phase 2: Collect entity positions, grouped by batch config
	//
	// Each chunk shares a const shared fragment (same archetype = same config).
	// Different chunks may have different configs → different batches.
	// -----------------------------------------------------------------------
	EntityQuery.ForEachEntityChunk(Context,
		[this, World](FMassExecutionContext& Ctx)
		{
			const FArcNiagaraBatchVisConfigFragment& Config =
				Ctx.GetConstSharedFragment<FArcNiagaraBatchVisConfigFragment>();

			UNiagaraSystem* System = Config.NiagaraSystem.LoadSynchronous();
			if (!System)
			{
				return;
			}

			FBatchState& Batch = GetOrCreateBatch(*World, *System, Config.PositionArrayParamName);

			const TConstArrayView<FTransformFragment> Transforms =
				Ctx.GetFragmentView<FTransformFragment>();

			const int32 NumEntities = Ctx.GetNumEntities();
			Batch.PositionBuffer.Reserve(Batch.PositionBuffer.Num() + NumEntities);

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				Batch.PositionBuffer.Add(Transforms[EntityIt].GetTransform().GetTranslation());
			}
		});

	// -----------------------------------------------------------------------
	// Phase 3: Push position arrays into each batch's Niagara component
	//
	// SetNiagaraArrayPosition writes into the DI's internal array and marks
	// it dirty. The component's next tick (next frame) picks up the new data
	// and pushes it to the render thread / GPU automatically.
	//
	// One frame of latency is inherent: the component ticked earlier this frame
	// with LAST frame's data. This is acceptable for smooth motion at 30-60fps.
	// -----------------------------------------------------------------------
	for (auto& [Key, State] : BatchStates)
	{
		UNiagaraComponent* Comp = State.NiagaraComponent.Get();
		if (!Comp)
		{
			continue;
		}

		// If no entities remain for this batch, push an empty array.
		// The Niagara system should handle 0-length gracefully (no particles).
		UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayPosition(
			Comp,
			State.PositionParamName,
			State.PositionBuffer
		);
	}
}

// ---------------------------------------------------------------------------
// GetOrCreateBatch
// ---------------------------------------------------------------------------

UArcNiagaraBatchVisProcessor::FBatchState& UArcNiagaraBatchVisProcessor::GetOrCreateBatch(
	UWorld& World, UNiagaraSystem& System, FName PositionParamName)
{
	TObjectKey<UNiagaraSystem> Key(&System);

	if (FBatchState* Existing = BatchStates.Find(Key))
	{
		if (Existing->NiagaraComponent.IsValid())
		{
			return *Existing;
		}
		// Component was destroyed externally (level unload, etc.) — recreate
	}

	FBatchState& State = BatchStates.FindOrAdd(Key);
	State.PositionParamName = PositionParamName;

	// Spawn a lightweight transient actor at origin to host the Niagara component.
	// One actor per batch (per unique Niagara system) — negligible overhead.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient; // Don't save with level

	AActor* Actor = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!Actor)
	{
		UE_LOG(LogMass, Error, TEXT("ArcNiagaraBatchVis: Failed to spawn batch owner actor for %s"),
			*GetNameSafe(&System));
		return State;
	}

#if WITH_EDITOR
	Actor->SetActorLabel(FString::Printf(TEXT("NiagaraBatch_%s"), *GetNameSafe(&System)));
#endif

	// Create the Niagara component — it self-ticks, self-renders, handles scene proxy.
	// We just push position data into it each frame via the DI function library.
	UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Actor);
	Comp->SetAsset(&System);
	Comp->SetAutoActivate(true);
	Comp->RegisterComponent();
	Comp->Activate(true);

	State.OwnerActor = Actor;
	State.NiagaraComponent = Comp;

	UE_LOG(LogMass, Log, TEXT("ArcNiagaraBatchVis: Created batch for system %s (PositionParam=%s)"),
		*GetNameSafe(&System), *PositionParamName.ToString());

	return State;
}
