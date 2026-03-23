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

		const int32 RadiusCells = Subsystem->GetSwapRadiusCells();
		const bool bIsFirstUpdate = (OldCell.X == TNumericLimits<int32>::Max());

		TArray<FIntVector> NewActiveCells;
		Subsystem->GetCellsInRadius(NewCell, RadiusCells, NewActiveCells);

		// On first update, all new cells are activated (no old cells to compare against)
		TSet<FIntVector> OldCellSet;
		if (!bIsFirstUpdate)
		{
			TArray<FIntVector> OldActiveCells;
			Subsystem->GetCellsInRadius(OldCell, RadiusCells, OldActiveCells);
			OldCellSet.Reserve(OldActiveCells.Num());
			for (const FIntVector& Cell : OldActiveCells)
			{
				OldCellSet.Add(Cell);
			}
		}

		TSet<FIntVector> NewCellSet;
		NewCellSet.Reserve(NewActiveCells.Num());
		for (const FIntVector& Cell : NewActiveCells)
		{
			NewCellSet.Add(Cell);
		}

		// --- ISM activation: all cells entering swap radius ---
		TArray<FMassEntityHandle> ActivatedEntities;
		for (const FIntVector& Cell : NewActiveCells)
		{
			if (!OldCellSet.Contains(Cell))
			{
				const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell);
				if (Entities)
				{
					ActivatedEntities.Append(*Entities);
				}
			}
		}

		// ISM deactivation: cells leaving swap radius
		TArray<FMassEntityHandle> DeactivatedEntities;
		if (!bIsFirstUpdate)
		{
			for (const FIntVector& Cell : OldCellSet)
			{
				if (!NewCellSet.Contains(Cell))
				{
					const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell);
					if (Entities)
					{
						DeactivatedEntities.Append(*Entities);
					}
				}
			}
		}

		if (ActivatedEntities.Num() > 0)
		{
			SignalSubsystem->SignalEntities(UE::ArcIW::Signals::CellActivated, ActivatedEntities);
		}

		if (DeactivatedEntities.Num() > 0)
		{
			SignalSubsystem->SignalEntities(UE::ArcIW::Signals::CellDeactivated, DeactivatedEntities);
		}

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

		Subsystem->UpdatePlayerCell(NewCell);
	}
}
