// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Persistence/ArcMassPersistenceProcessors.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMass/Persistence/ArcMassEntityPersistenceSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"

// ── Source Tracking Processor ────────────────────────────────────────────

UArcMassPersistenceSourceTrackingProcessor::
	UArcMassPersistenceSourceTrackingProcessor()
	: SourceQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server);
}

void UArcMassPersistenceSourceTrackingProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	SourceQuery.AddTagRequirement<FArcMassPersistenceSourceTag>(
		EMassFragmentPresence::All);
	SourceQuery.AddRequirement<FTransformFragment>(
		EMassFragmentAccess::ReadOnly);
}

void UArcMassPersistenceSourceTrackingProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UArcMassEntityPersistenceSubsystem* Subsystem =
		EntityManager.GetWorld()->GetSubsystem<
			UArcMassEntityPersistenceSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassPersistenceSourceTracking);

	TArray<FVector> SourcePositions;

	SourceQuery.ForEachEntityChunk(Context,
		[&SourcePositions](FMassExecutionContext& Ctx)
		{
			const auto Transforms =
				Ctx.GetFragmentView<FTransformFragment>();
			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				SourcePositions.Add(
					Transforms[i].GetTransform().GetLocation());
			}
		});

	if (SourcePositions.Num() > 0)
	{
		Subsystem->UpdateActiveCells(SourcePositions);
	}
}

// ── Cell Tracking Processor ──────────────────────────────────────────────

UArcMassPersistenceCellTrackingProcessor::
	UArcMassPersistenceCellTrackingProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server);
	ExecutionOrder.ExecuteAfter.Add(
		UArcMassPersistenceSourceTrackingProcessor::StaticClass()->GetFName());
}

void UArcMassPersistenceCellTrackingProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(
		EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcMassPersistenceFragment>(
		EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FArcMassPersistenceTag>(
		EMassFragmentPresence::All);
}

void UArcMassPersistenceCellTrackingProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UArcMassEntityPersistenceSubsystem* Subsystem =
		EntityManager.GetWorld()->GetSubsystem<
			UArcMassEntityPersistenceSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassPersistenceCellTracking);

	const float CellSize = Subsystem->GetCellSize();

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem, CellSize](FMassExecutionContext& Ctx)
		{
			const auto Transforms =
				Ctx.GetFragmentView<FTransformFragment>();
			auto PersistFragments =
				Ctx.GetMutableFragmentView<FArcMassPersistenceFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				const FVector Pos =
					Transforms[i].GetTransform().GetLocation();
				const FIntVector NewCell =
					UE::ArcMass::Persistence::WorldToCell(Pos, CellSize);

				FArcMassPersistenceFragment& PFrag = PersistFragments[i];
				if (NewCell != PFrag.CurrentCell)
				{
					const FIntVector OldCell = PFrag.CurrentCell;
					PFrag.CurrentCell = NewCell;
					Subsystem->OnEntityCellChanged(
						PFrag.PersistenceGuid, OldCell, NewCell);
				}
			}
		});
}

// ── Init Observer ────────────────────────────────────────────────────────

UArcMassPersistenceInitObserver::UArcMassPersistenceInitObserver()
	: EntityQuery{*this}
{
	ObservedTypes.Add(FArcMassPersistenceTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcMassPersistenceInitObserver::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassPersistenceFragment>(
		EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(
		EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FArcMassPersistenceTag>(
		EMassFragmentPresence::All);
}

void UArcMassPersistenceInitObserver::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UArcMassEntityPersistenceSubsystem* Subsystem =
		EntityManager.GetWorld()->GetSubsystem<
			UArcMassEntityPersistenceSubsystem>();

	const float CellSize = Subsystem ? Subsystem->GetCellSize() : 10000.f;

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassPersistenceInit);

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem, CellSize](FMassExecutionContext& Ctx)
		{
			const auto Transforms =
				Ctx.GetFragmentView<FTransformFragment>();
			auto PersistFragments =
				Ctx.GetMutableFragmentView<FArcMassPersistenceFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				FArcMassPersistenceFragment& PFrag = PersistFragments[i];

				// Assign GUID if not loaded from save
				if (!PFrag.PersistenceGuid.IsValid())
				{
					PFrag.PersistenceGuid = FGuid::NewGuid();
				}

				// Set initial cell
				const FVector Pos =
					Transforms[i].GetTransform().GetLocation();
				PFrag.CurrentCell =
					UE::ArcMass::Persistence::WorldToCell(Pos, CellSize);

				if (PFrag.StorageCell ==
					FIntVector(MAX_int32, MAX_int32, MAX_int32))
				{
					PFrag.StorageCell = PFrag.CurrentCell;
				}

				// Register with subsystem
				if (Subsystem)
				{
					Subsystem->OnEntityCellChanged(
						PFrag.PersistenceGuid,
						FIntVector(MAX_int32, MAX_int32, MAX_int32),
						PFrag.CurrentCell);

					// Register handle for serialization (loaded entities
					// already registered in SpawnParsedEntities — guard
					// prevents overwriting their handle)
					if (!Subsystem->ActiveEntities.Contains(
							PFrag.PersistenceGuid))
					{
						Subsystem->ActiveEntities.Add(
							PFrag.PersistenceGuid,
							Ctx.GetEntity(i));
					}
				}
			}
		});
}

// ── Deinit Observer ─────────────────────────────────────────────────────

UArcMassPersistenceDeinitObserver::UArcMassPersistenceDeinitObserver()
	: EntityQuery{*this}
{
	ObservedTypes.Add(FArcMassPersistenceTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcMassPersistenceDeinitObserver::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassPersistenceFragment>(
		EMassFragmentAccess::ReadOnly);
}

void UArcMassPersistenceDeinitObserver::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UArcMassEntityPersistenceSubsystem* Subsystem =
		EntityManager.GetWorld()->GetSubsystem<
			UArcMassEntityPersistenceSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassPersistenceDeinit);

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FArcMassPersistenceFragment> PersistFragments =
				Ctx.GetFragmentView<FArcMassPersistenceFragment>();

			for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
			{
				const FArcMassPersistenceFragment& PFrag = PersistFragments[i];
				const FGuid& Guid = PFrag.PersistenceGuid;

				if (!Guid.IsValid())
				{
					continue;
				}

				Subsystem->ActiveEntities.Remove(Guid);

				if (TSet<FGuid>* CellSet =
						Subsystem->CellEntityMap.Find(PFrag.CurrentCell))
				{
					CellSet->Remove(Guid);
					if (CellSet->IsEmpty())
					{
						Subsystem->CellEntityMap.Remove(PFrag.CurrentCell);
					}
				}
			}
		});
}
