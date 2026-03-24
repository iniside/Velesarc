// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWPlayerCellTrackingProcessor.h"

#include "ArcIWTypes.h"
#include "ArcIWVisualizationSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/ArcMassEntityVisualization.h"
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
		const FIntVector NewCell = Subsystem->WorldToCell(SourceLocation);
		const FIntVector OldCell = Subsystem->GetLastPlayerCell();

		if (NewCell == OldCell)
		{
			continue;
		}

		const bool bIsFirstUpdate = (OldCell.X == TNumericLimits<int32>::Max());

		// --- Actor hydration: inner radius ---
		const int32 ActorRadiusCells = Subsystem->GetActorRadiusCells();

		TArray<FIntVector> NewActorCells;
		Subsystem->GetCellsInRadius(NewCell, ActorRadiusCells, NewActorCells);

		TSet<FIntVector> OldActorCellSet;
		if (!bIsFirstUpdate)
		{
			TArray<FIntVector> OldActorCells;
			Subsystem->GetCellsInRadius(OldCell, ActorRadiusCells, OldActorCells);
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
				const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell);
				if (Entities)
				{
					ActorActivatedEntities.Append(*Entities);
				}
			}
		}

		TArray<FMassEntityHandle> ActorDeactivatedEntities;
		if (!bIsFirstUpdate)
		{
			for (const FIntVector& Cell : OldActorCellSet)
			{
				if (!NewActorCellSet.Contains(Cell))
				{
					const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell);
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

		// --- MassISM Mesh radius (hysteresis) ---
		{
			TArray<FIntVector> NewMeshAddCells;
			Subsystem->GetCellsInRadius(NewCell, Subsystem->GetMeshAddRadiusCells(), NewMeshAddCells);

			TSet<FIntVector> OldMeshAddCellSet;
			if (!bIsFirstUpdate)
			{
				TArray<FIntVector> OldMeshAddCells;
				Subsystem->GetCellsInRadius(OldCell, Subsystem->GetMeshAddRadiusCells(), OldMeshAddCells);
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
					const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell);
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
			Subsystem->GetCellsInRadius(NewCell, Subsystem->GetMeshRemoveRadiusCells(), NewMeshRemoveCells);

			TSet<FIntVector> OldMeshRemoveCellSet;
			if (!bIsFirstUpdate)
			{
				TArray<FIntVector> OldMeshRemoveCells;
				Subsystem->GetCellsInRadius(OldCell, Subsystem->GetMeshRemoveRadiusCells(), OldMeshRemoveCells);
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
			if (!bIsFirstUpdate)
			{
				for (const FIntVector& Cell : OldMeshRemoveCellSet)
				{
					if (!NewMeshRemoveCellSet.Contains(Cell))
					{
						const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell);
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
		{
			TArray<FIntVector> NewPhysicsAddCells;
			Subsystem->GetCellsInRadius(NewCell, Subsystem->GetPhysicsAddRadiusCells(), NewPhysicsAddCells);

			TSet<FIntVector> OldPhysicsAddCellSet;
			if (!bIsFirstUpdate)
			{
				TArray<FIntVector> OldPhysicsAddCells;
				Subsystem->GetCellsInRadius(OldCell, Subsystem->GetPhysicsAddRadiusCells(), OldPhysicsAddCells);
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
					const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell);
					if (Entities)
					{
						PhysicsActivatedEntities.Append(*Entities);
					}
				}
			}

			if (PhysicsActivatedEntities.Num() > 0)
			{
				SignalSubsystem->SignalEntities(UE::ArcIW::Signals::PhysicsCellActivated, PhysicsActivatedEntities);
			}

			TArray<FIntVector> NewPhysicsRemoveCells;
			Subsystem->GetCellsInRadius(NewCell, Subsystem->GetPhysicsRemoveRadiusCells(), NewPhysicsRemoveCells);

			TSet<FIntVector> OldPhysicsRemoveCellSet;
			if (!bIsFirstUpdate)
			{
				TArray<FIntVector> OldPhysicsRemoveCells;
				Subsystem->GetCellsInRadius(OldCell, Subsystem->GetPhysicsRemoveRadiusCells(), OldPhysicsRemoveCells);
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
			if (!bIsFirstUpdate)
			{
				for (const FIntVector& Cell : OldPhysicsRemoveCellSet)
				{
					if (!NewPhysicsRemoveCellSet.Contains(Cell))
					{
						const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell);
						if (Entities)
						{
							PhysicsDeactivatedEntities.Append(*Entities);
						}
					}
				}
			}

			if (PhysicsDeactivatedEntities.Num() > 0)
			{
				SignalSubsystem->SignalEntities(UE::ArcIW::Signals::PhysicsCellDeactivated, PhysicsDeactivatedEntities);
			}
		}

		Subsystem->UpdatePlayerCell(NewCell);
	}
}
