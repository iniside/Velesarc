// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Processors/ArcIWPlayerCellTrackingProcessor.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWPlayerCellTrackingProcessor)

// ---------------------------------------------------------------------------
// UArcIWPlayerCellTrackingProcessor
// Iterates source entities (FArcVisSourceEntityTag + FTransformFragment) to
// drive ISM cell activation/deactivation, matching the ArcMass pattern.
// ---------------------------------------------------------------------------

UArcIWPlayerCellTrackingProcessor::UArcIWPlayerCellTrackingProcessor()
	: SourceEntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWPlayerCellTrackingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// Query source entities if available (Mass entities with streaming tag).
	SourceEntityQuery.AddTagRequirement<FArcVisSourceEntityTag>(EMassFragmentPresence::All);
	SourceEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcIWPlayerCellTrackingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcIWVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcIWVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWPlayerCellTracking);

	// Collect source positions: from Mass source entities first, fallback to player camera
	TArray<FVector> SourcePositions;

	SourceEntityQuery.ForEachEntityChunk(Context,
		[&SourcePositions](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				SourcePositions.Add(Transforms[EntityIt].GetTransform().GetLocation());
			}
		});

	// Fallback: if no source entities, use player camera location
	if (SourcePositions.IsEmpty())
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FVector CameraLocation;
			FRotator CameraRotation;
			PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
			SourcePositions.Add(CameraLocation);
		}
	}

	if (SourcePositions.IsEmpty())
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSubsystem)
	{
		return;
	}

	// Process each source position
	for (const FVector& SourceLocation : SourcePositions)
	{
		const FIntVector NewMeshCell = Subsystem->WorldToMeshCell(SourceLocation);
		const FIntVector OldMeshCell = Subsystem->GetLastMeshPlayerCell();
		const FIntVector NewPhysicsCell = Subsystem->WorldToPhysicsCell(SourceLocation);
		const FIntVector OldPhysicsCell = Subsystem->GetLastPhysicsPlayerCell();
		const FIntVector NewActorCell = Subsystem->WorldToActorCell(SourceLocation);
		const FIntVector OldActorCell = Subsystem->GetLastActorPlayerCell();

		const bool bMeshCellChanged = (NewMeshCell != OldMeshCell);
		const bool bPhysicsCellChanged = (NewPhysicsCell != OldPhysicsCell);
		const bool bActorCellChanged = (NewActorCell != OldActorCell);

		if (!bMeshCellChanged && !bPhysicsCellChanged && !bActorCellChanged)
		{
			continue;
		}

		const bool bIsFirstMeshUpdate = (OldMeshCell.X == TNumericLimits<int32>::Max());
		const bool bIsFirstPhysicsUpdate = (OldPhysicsCell.X == TNumericLimits<int32>::Max());
		const bool bIsFirstActorUpdate = (OldActorCell.X == TNumericLimits<int32>::Max());

		// --- Actor hydration radius ---
		if (Subsystem->HasActorGrid() && bActorCellChanged)
		{
			const int32 ActorHydrationRadiusCells = Subsystem->GetActorHydrationRadiusCells();

			TArray<FIntVector> NewActorCells;
			UArcIWVisualizationSubsystem::GetCellsInRadius(NewActorCell, ActorHydrationRadiusCells, NewActorCells);

			TSet<FIntVector> OldActorCellSet;
			if (!bIsFirstActorUpdate)
			{
				TArray<FIntVector> OldActorCells;
				UArcIWVisualizationSubsystem::GetCellsInRadius(OldActorCell, ActorHydrationRadiusCells, OldActorCells);
				OldActorCellSet.Reserve(OldActorCells.Num());
				for (const FIntVector& Cell : OldActorCells)
				{
					OldActorCellSet.Add(Cell);
				}
			}

			TSet<FIntVector> NewActorCellSet;
			NewActorCellSet.Reserve(NewActorCells.Num());
			for (const FIntVector& Cell : NewActorCells)
			{
				NewActorCellSet.Add(Cell);
			}

			TArray<FMassEntityHandle> ActorActivatedEntities;
			for (const FIntVector& Cell : NewActorCells)
			{
				if (!OldActorCellSet.Contains(Cell))
				{
					const TArray<FMassEntityHandle>* Entities = Subsystem->GetActorEntitiesInCell(Cell);
					if (Entities)
					{
						ActorActivatedEntities.Append(*Entities);
					}
				}
			}

			TArray<FMassEntityHandle> ActorDeactivatedEntities;
			if (!bIsFirstActorUpdate)
			{
				for (const FIntVector& Cell : OldActorCellSet)
				{
					if (!NewActorCellSet.Contains(Cell))
					{
						const TArray<FMassEntityHandle>* Entities = Subsystem->GetActorEntitiesInCell(Cell);
						if (Entities)
						{
							ActorDeactivatedEntities.Append(*Entities);
						}
					}
				}
			}

			if (ActorActivatedEntities.Num() > 0)
			{
				SignalSubsystem->SignalEntities(UE::ArcIW::Signals::ActorCellActivated, ActorActivatedEntities);
			}
			if (ActorDeactivatedEntities.Num() > 0)
			{
				SignalSubsystem->SignalEntities(UE::ArcIW::Signals::ActorCellDeactivated, ActorDeactivatedEntities);
			}
		}

		// --- MassISM Mesh radius (hysteresis) ---
		if (bMeshCellChanged)
		{
			TArray<FIntVector> NewMeshAddCells;
			UArcIWVisualizationSubsystem::GetCellsInRadius(NewMeshCell, Subsystem->GetMeshAddRadiusCells(), NewMeshAddCells);

			TSet<FIntVector> OldMeshAddCellSet;
			if (!bIsFirstMeshUpdate)
			{
				TArray<FIntVector> OldMeshAddCells;
				UArcIWVisualizationSubsystem::GetCellsInRadius(OldMeshCell, Subsystem->GetMeshAddRadiusCells(), OldMeshAddCells);
				OldMeshAddCellSet.Reserve(OldMeshAddCells.Num());
				for (const FIntVector& Cell : OldMeshAddCells)
				{
					OldMeshAddCellSet.Add(Cell);
				}
			}

			TArray<FMassEntityHandle> MeshActivatedEntities;
			for (const FIntVector& Cell : NewMeshAddCells)
			{
				if (!OldMeshAddCellSet.Contains(Cell))
				{
					const TArray<FMassEntityHandle>* Entities = Subsystem->GetMeshEntitiesInCell(Cell);
					if (Entities)
					{
						MeshActivatedEntities.Append(*Entities);
					}
				}
			}

			if (MeshActivatedEntities.Num() > 0)
			{
				SignalSubsystem->SignalEntities(UE::ArcIW::Signals::MeshCellActivated, MeshActivatedEntities);
			}

			TArray<FIntVector> NewMeshRemoveCells;
			UArcIWVisualizationSubsystem::GetCellsInRadius(NewMeshCell, Subsystem->GetMeshRemoveRadiusCells(), NewMeshRemoveCells);

			TSet<FIntVector> OldMeshRemoveCellSet;
			if (!bIsFirstMeshUpdate)
			{
				TArray<FIntVector> OldMeshRemoveCells;
				UArcIWVisualizationSubsystem::GetCellsInRadius(OldMeshCell, Subsystem->GetMeshRemoveRadiusCells(), OldMeshRemoveCells);
				OldMeshRemoveCellSet.Reserve(OldMeshRemoveCells.Num());
				for (const FIntVector& Cell : OldMeshRemoveCells)
				{
					OldMeshRemoveCellSet.Add(Cell);
				}
			}

			TSet<FIntVector> NewMeshRemoveCellSet;
			NewMeshRemoveCellSet.Reserve(NewMeshRemoveCells.Num());
			for (const FIntVector& Cell : NewMeshRemoveCells)
			{
				NewMeshRemoveCellSet.Add(Cell);
			}

			TArray<FMassEntityHandle> MeshDeactivatedEntities;
			if (!bIsFirstMeshUpdate)
			{
				for (const FIntVector& Cell : OldMeshRemoveCellSet)
				{
					if (!NewMeshRemoveCellSet.Contains(Cell))
					{
						const TArray<FMassEntityHandle>* Entities = Subsystem->GetMeshEntitiesInCell(Cell);
						if (Entities)
						{
							MeshDeactivatedEntities.Append(*Entities);
						}
					}
				}
			}

			if (MeshDeactivatedEntities.Num() > 0)
			{
				SignalSubsystem->SignalEntities(UE::ArcIW::Signals::MeshCellDeactivated, MeshDeactivatedEntities);
			}
		}

		// --- MassISM Physics radius (hysteresis) ---
		if (bPhysicsCellChanged)
		{
			TArray<FIntVector> NewPhysicsAddCells;
			UArcIWVisualizationSubsystem::GetCellsInRadius(NewPhysicsCell, Subsystem->GetPhysicsAddRadiusCells(), NewPhysicsAddCells);

			TSet<FIntVector> OldPhysicsAddCellSet;
			if (!bIsFirstPhysicsUpdate)
			{
				TArray<FIntVector> OldPhysicsAddCells;
				UArcIWVisualizationSubsystem::GetCellsInRadius(OldPhysicsCell, Subsystem->GetPhysicsAddRadiusCells(), OldPhysicsAddCells);
				OldPhysicsAddCellSet.Reserve(OldPhysicsAddCells.Num());
				for (const FIntVector& Cell : OldPhysicsAddCells)
				{
					OldPhysicsAddCellSet.Add(Cell);
				}
			}

			TArray<FMassEntityHandle> PhysicsActivatedEntities;
			for (const FIntVector& Cell : NewPhysicsAddCells)
			{
				if (!OldPhysicsAddCellSet.Contains(Cell))
				{
					const TArray<FMassEntityHandle>* Entities = Subsystem->GetPhysicsEntitiesInCell(Cell);
					if (Entities)
					{
						PhysicsActivatedEntities.Append(*Entities);
					}
				}
			}

			if (PhysicsActivatedEntities.Num() > 0)
			{
				SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, PhysicsActivatedEntities);
			}

			TArray<FIntVector> NewPhysicsRemoveCells;
			UArcIWVisualizationSubsystem::GetCellsInRadius(NewPhysicsCell, Subsystem->GetPhysicsRemoveRadiusCells(), NewPhysicsRemoveCells);

			TSet<FIntVector> OldPhysicsRemoveCellSet;
			if (!bIsFirstPhysicsUpdate)
			{
				TArray<FIntVector> OldPhysicsRemoveCells;
				UArcIWVisualizationSubsystem::GetCellsInRadius(OldPhysicsCell, Subsystem->GetPhysicsRemoveRadiusCells(), OldPhysicsRemoveCells);
				OldPhysicsRemoveCellSet.Reserve(OldPhysicsRemoveCells.Num());
				for (const FIntVector& Cell : OldPhysicsRemoveCells)
				{
					OldPhysicsRemoveCellSet.Add(Cell);
				}
			}

			TSet<FIntVector> NewPhysicsRemoveCellSet;
			NewPhysicsRemoveCellSet.Reserve(NewPhysicsRemoveCells.Num());
			for (const FIntVector& Cell : NewPhysicsRemoveCells)
			{
				NewPhysicsRemoveCellSet.Add(Cell);
			}

			TArray<FMassEntityHandle> PhysicsDeactivatedEntities;
			if (!bIsFirstPhysicsUpdate)
			{
				for (const FIntVector& Cell : OldPhysicsRemoveCellSet)
				{
					if (!NewPhysicsRemoveCellSet.Contains(Cell))
					{
						const TArray<FMassEntityHandle>* Entities = Subsystem->GetPhysicsEntitiesInCell(Cell);
						if (Entities)
						{
							PhysicsDeactivatedEntities.Append(*Entities);
						}
					}
				}
			}

			if (PhysicsDeactivatedEntities.Num() > 0)
			{
				SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyReleased, PhysicsDeactivatedEntities);
			}
		}

		if (bMeshCellChanged)
		{
			Subsystem->UpdateMeshPlayerCell(NewMeshCell);
		}
		if (bPhysicsCellChanged)
		{
			Subsystem->UpdatePhysicsPlayerCell(NewPhysicsCell);
		}
		if (bActorCellChanged)
		{
			Subsystem->UpdateActorPlayerCell(NewActorCell);
		}
	}
}
