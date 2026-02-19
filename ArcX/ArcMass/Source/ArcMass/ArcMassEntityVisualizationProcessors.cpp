// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityVisualizationProcessors.h"

#include "ArcMassEntityVisualization.h"
#include "ArcVisEntityComponent.h"
#include "DrawDebugHelpers.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

// ---------------------------------------------------------------------------
// UArcVisPlayerCellTrackingProcessor
// ---------------------------------------------------------------------------

UArcVisPlayerCellTrackingProcessor::UArcVisPlayerCellTrackingProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcVisPlayerCellTrackingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// This processor doesn't iterate entities in its query — it reads player position and signals.
	// We still need a dummy query for the processor to be valid.
	EntityQuery.AddTagRequirement<FArcVisSourceEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcVisPlayerCellTrackingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}
	const FArcVisualizationGrid& Grid = Subsystem->GetGrid();
	
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, &Grid] (FMassExecutionContext& Ctx)
			{
				TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
				for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
				{
					const FTransformFragment& TM = Transforms[EntityIt];

					
					const FVector PlayerLocation = TM.GetTransform().GetLocation();
					const FIntVector NewCell = Grid.WorldToCell(PlayerLocation);
					const FIntVector OldCell = Subsystem->GetLastPlayerCell();

					if (NewCell != OldCell)
					{
						// Compute old and new active cell sets
						const int32 RadiusCells = Subsystem->GetSwapRadiusCells();

						TArray<FIntVector> OldActiveCells;
						TArray<FIntVector> NewActiveCells;
						Grid.GetCellsInRadius(OldCell, RadiusCells, OldActiveCells);
						Grid.GetCellsInRadius(NewCell, RadiusCells, NewActiveCells);

						// Convert to sets for fast lookup
						TSet<FIntVector> OldCellSet;
						OldCellSet.Reserve(OldActiveCells.Num());
						for (const FIntVector& Cell : OldActiveCells)
						{
							OldCellSet.Add(Cell);
						}

						TSet<FIntVector> NewCellSet;
						NewCellSet.Reserve(NewActiveCells.Num());
						for (const FIntVector& Cell : NewActiveCells)
						{
							NewCellSet.Add(Cell);
						}

						UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
						if (!SignalSubsystem)
						{
							return;
						}

						// Cells in new set but not old → activated
						TArray<FMassEntityHandle> ActivatedEntities;
						for (const FIntVector& Cell : NewActiveCells)
						{
							if (!OldCellSet.Contains(Cell))
							{
								if (const TArray<FMassEntityHandle>* Entities = Grid.GetEntitiesInCell(Cell))
								{
									ActivatedEntities.Append(*Entities);
								}
							}
						}

						// Cells in old but not new → deactivated
						TArray<FMassEntityHandle> DeactivatedEntities;
						for (const FIntVector& Cell : OldActiveCells)
						{
							if (!NewCellSet.Contains(Cell))
							{
								if (const TArray<FMassEntityHandle>* Entities = Grid.GetEntitiesInCell(Cell))
								{
									DeactivatedEntities.Append(*Entities);
								}
							}
						}

						if (ActivatedEntities.Num() > 0)
						{
							SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisualizationCellActivated, ActivatedEntities);
						}

						if (DeactivatedEntities.Num() > 0)
						{
							SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisualizationCellDeactivated, DeactivatedEntities);
						}

						Subsystem->UpdatePlayerCell(NewCell);
					}

#if WITH_GAMEPLAY_DEBUGGER
					extern TAutoConsoleVariable<bool> CVarArcDebugDrawVisualizationGrid;
					if (CVarArcDebugDrawVisualizationGrid.GetValueOnAnyThread())
					{
						const float CellSize = Grid.CellSize;
						const float HalfCell = CellSize * 0.5f;
						const FIntVector CurrentPlayerCell = Subsystem->GetLastPlayerCell();

						// Draw swap radius sphere around player
						DrawDebugSphere(World, PlayerLocation, Subsystem->GetSwapRadius(), 32, FColor::White, false, -1.f, 0, 1.f);

						// Draw all cells that contain entities, colored by state:
						//   Green  = Actor representation (inside swap radius)
						//   Blue   = ISM representation (outside swap radius)
						//   Yellow = Active cell, empty (no entities)
						for (const auto& CellPair : Grid.CellEntities)
						{
							const FIntVector& CellCoord = CellPair.Key;
							const TArray<FMassEntityHandle>& Entities = CellPair.Value;

							const FVector CellCenter(
								CellCoord.X * CellSize + HalfCell,
								CellCoord.Y * CellSize + HalfCell,
								CellCoord.Z * CellSize + HalfCell
							);

							// Determine cell state from first valid entity
							bool bIsActorCell = false;
							for (const FMassEntityHandle& Entity : Entities)
							{
								if (EntityManager.IsEntityValid(Entity))
								{
									const FArcVisRepresentationFragment* Rep = EntityManager.GetFragmentDataPtr<
										FArcVisRepresentationFragment>(Entity);
									if (Rep)
									{
										bIsActorCell = Rep->bIsActorRepresentation;
										break;
									}
								}
							}

							const FColor DrawColor = bIsActorCell ?
									FColor::Green :
									FColor::Blue;
							DrawDebugBox(World, CellCenter, FVector(HalfCell * 0.9f), DrawColor, false, -1.f, 0, 2.f);

							// Draw entity count label
							const FString CountLabel = FString::Printf(TEXT("%d %s"), Entities.Num(), bIsActorCell ?
								TEXT("ACT") :
								TEXT("ISM"));
							DrawDebugString(World, CellCenter + FVector(0, 0, HalfCell * 0.7f), CountLabel, nullptr, bIsActorCell ?
								FColor::Green :
								FColor::Cyan, -1.f, true, 1.f);
						}

						// Draw active cell boundary (cells within swap radius that are empty)
						const int32 RadiusCells = Subsystem->GetSwapRadiusCells();
						TArray<FIntVector> ActiveCells;
						Grid.GetCellsInRadius(CurrentPlayerCell, RadiusCells, ActiveCells);

						for (const FIntVector& Cell : ActiveCells)
						{
							if (!Grid.GetEntitiesInCell(Cell))
							{
								continue;
							}

							// Draw thin outline for active boundary cells that are on the edge
							const int32 DX = FMath::Abs(Cell.X - CurrentPlayerCell.X);
							const int32 DY = FMath::Abs(Cell.Y - CurrentPlayerCell.Y);
							const int32 DZ = FMath::Abs(Cell.Z - CurrentPlayerCell.Z);
							if (DX == RadiusCells || DY == RadiusCells || DZ == RadiusCells)
							{
								const FVector CellCenter(
									Cell.X * CellSize + HalfCell,
									Cell.Y * CellSize + HalfCell,
									Cell.Z * CellSize + HalfCell
								);
								DrawDebugBox(World, CellCenter, FVector(HalfCell), FColor::Orange, false, -1.f, 0, 1.f);
							}
						}

						// Draw player cell highlight
						const FVector PlayerCellCenter(
							CurrentPlayerCell.X * CellSize + HalfCell,
							CurrentPlayerCell.Y * CellSize + HalfCell,
							CurrentPlayerCell.Z * CellSize + HalfCell
						);
						DrawDebugBox(World, PlayerCellCenter, FVector(HalfCell * 0.95f), FColor::White, false, -1.f, 0, 3.f);
					}
#endif
				}
			}
	);
	
}

// ---------------------------------------------------------------------------
// UArcVisActivateProcessor — ISM → Actor
// ---------------------------------------------------------------------------

UArcVisActivateProcessor::UArcVisActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisualizationCellActivated);
}

void UArcVisActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
}

void UArcVisActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> RepFragments = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];
				if (Rep.bIsActorRepresentation)
				{
					continue;
				}

				const FTransform& EntityTransform = TransformFragments[EntityIt].GetTransform();

				// Remove ISM instance
				if (Config.StaticMesh)
				{
					if (Rep.ISMInstanceId != INDEX_NONE)
					{
						Subsystem->RemoveISMInstance(Rep.GridCoords, Config.ISMManagerClass, Config.StaticMesh, Rep.ISMInstanceId, EntityManager);
						Rep.ISMInstanceId = INDEX_NONE;
					}
				}

				// Spawn actor
				if (Config.ActorClass)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					AActor* NewActor = World->SpawnActor<AActor>(Config.ActorClass, EntityTransform, SpawnParams);

					const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
					MassActorFragment.SetAndUpdateHandleMap(Entity, NewActor, true);

					if (UArcVisEntityComponent* VisComp = NewActor ? NewActor->FindComponentByClass<UArcVisEntityComponent>() : nullptr)
					{
						VisComp->NotifyVisActorCreated(Entity);
					}

					NewActor->FinishSpawning(EntityTransform);
				}

				Rep.bIsActorRepresentation = true;
			}
		});
}

// ---------------------------------------------------------------------------
// UArcVisDeactivateProcessor — Actor → ISM
// ---------------------------------------------------------------------------

UArcVisDeactivateProcessor::UArcVisDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisualizationCellDeactivated);
}

void UArcVisDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
}

void UArcVisDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> RepFragments = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];
				if (!Rep.bIsActorRepresentation)
				{
					continue;
				}

				// Notify component before destroying actor
				if (AActor* Actor = MassActorFragment.GetMutable())
				{
					if (UArcVisEntityComponent* VisComp = Actor->FindComponentByClass<UArcVisEntityComponent>())
					{
						VisComp->NotifyVisActorPreDestroy();
					}
					Actor->Destroy();
				}
				MassActorFragment.ResetAndUpdateHandleMap();

				// Add ISM instance
				if (Config.StaticMesh)
				{
					const FTransform& EntityTransform = TransformFragments[EntityIt].GetTransform();
				
					const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
					Rep.ISMInstanceId = Subsystem->AddISMInstance(
						Rep.GridCoords,
						Config.ISMManagerClass,
						Config.StaticMesh,
						Config.MaterialOverrides,
						Config.bCastShadows,
						EntityTransform,
						Entity);
				}

				Rep.bIsActorRepresentation = false;
			}
		});
}

// ---------------------------------------------------------------------------
// UArcVisEntityInitObserver
// ---------------------------------------------------------------------------

UArcVisEntityInitObserver::UArcVisEntityInitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcVisEntityTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcVisEntityInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
}

void UArcVisEntityInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> RepFragments = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];
				const FTransform& EntityTransform = TransformFragments[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Compute and store grid coords
				const FVector Position = EntityTransform.GetLocation();
				Rep.GridCoords = Subsystem->GetGrid().WorldToCell(Position);

				// Register in the grid
				Subsystem->RegisterEntity(Entity, Position);

				// Check if this entity's cell is already active (player is nearby)
				if (Subsystem->IsActiveCellCoord(Rep.GridCoords))
				{
					// Start in actor mode if player is nearby
					if (Config.ActorClass)
					{
						UWorld* World = Ctx.GetWorld();
						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						SpawnParams.bDeferConstruction = true;
						AActor* NewActor = World->SpawnActor<AActor>(Config.ActorClass, EntityTransform, SpawnParams);
						MassActorFragment.SetAndUpdateHandleMap(Entity, NewActor, true);

						if (UArcVisEntityComponent* VisComp = NewActor ? NewActor->FindComponentByClass<UArcVisEntityComponent>() : nullptr)
						{
							VisComp->NotifyVisActorCreated(Entity);
						}

						NewActor->FinishSpawning(EntityTransform);
					}
					Rep.bIsActorRepresentation = true;
				}
				else
				{
					// Start in ISM mode
					if (Config.StaticMesh)
					{
						Rep.ISMInstanceId = Subsystem->AddISMInstance(
							Rep.GridCoords,
							Config.ISMManagerClass,
							Config.StaticMesh,
							Config.MaterialOverrides,
							Config.bCastShadows,
							EntityTransform,
							Entity);
						
					}
					Rep.bIsActorRepresentation = false;
				}
			}
		});
}

// ---------------------------------------------------------------------------
// UArcVisEntityDeinitObserver
// ---------------------------------------------------------------------------

UArcVisEntityDeinitObserver::UArcVisEntityDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcVisEntityTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcVisEntityDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
}

void UArcVisEntityDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> RepFragments = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				if (Rep.bIsActorRepresentation)
				{
					if (AActor* Actor = MassActorFragment.GetMutable())
					{
						if (UArcVisEntityComponent* VisComp = Actor->FindComponentByClass<UArcVisEntityComponent>())
						{
							VisComp->NotifyVisActorPreDestroy();
						}
						Actor->Destroy();
					}
					MassActorFragment.ResetAndUpdateHandleMap();
				}
				else if (Config.StaticMesh)
				{
					if (Rep.ISMInstanceId != INDEX_NONE)
					{
						Subsystem->RemoveISMInstance(Rep.GridCoords, Config.ISMManagerClass, Config.StaticMesh, Rep.ISMInstanceId, EntityManager);
						Rep.ISMInstanceId = INDEX_NONE;
					}
				}

				Subsystem->UnregisterEntity(Entity, Rep.GridCoords);
			}
		});
}
