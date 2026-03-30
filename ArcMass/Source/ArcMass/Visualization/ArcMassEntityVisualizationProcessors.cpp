// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityVisualizationProcessors.h"

#include "ArcMassEntityVisualization.h"
#include "ArcVisEntityComponent.h"
#include "ArcVisLifecycle.h"
#include "ArcVisSettings.h"
#include "DrawDebugHelpers.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommands.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "Engine/StaticMesh.h"

// MassEngine mesh rendering
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMassVisualizationConfigFragments.h"

// Physics
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
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
	const FArcVisualizationGrid& MeshGrid = Subsystem->GetMeshGrid();
	const FArcVisualizationGrid& PhysicsGrid = Subsystem->GetPhysicsGrid();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisPlayerCellTracking);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, &MeshGrid, &PhysicsGrid] (FMassExecutionContext& Ctx)
		{
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FTransformFragment& TM = Transforms[EntityIt];
				const FVector PlayerLocation = TM.GetTransform().GetLocation();
				const FIntVector NewCell = MeshGrid.WorldToCell(PlayerLocation);
				const FIntVector OldCell = Subsystem->GetLastMeshPlayerCell();

				if (NewCell != OldCell)
				{
					const int32 ActivationRadiusCells = Subsystem->GetMeshActivationRadiusCells();
					const int32 DeactivationRadiusCells = Subsystem->GetMeshDeactivationRadiusCells();
					const FIntVector SentinelCell = FIntVector(TNumericLimits<int32>::Max());
					const bool bIsFirstUpdate = (OldCell == SentinelCell);

					// Compute four cell sets for hysteresis
					TArray<FIntVector> NewActivationCells;
					TArray<FIntVector> NewDeactivationCells;
					MeshGrid.GetCellsInRadius(NewCell, ActivationRadiusCells, NewActivationCells);
					MeshGrid.GetCellsInRadius(NewCell, DeactivationRadiusCells, NewDeactivationCells);

					TSet<FIntVector> OldDeactivationSet;
					TSet<FIntVector> NewDeactivationSet;

					// Build NewDeactivation set
					NewDeactivationSet.Reserve(NewDeactivationCells.Num());
					for (const FIntVector& Cell : NewDeactivationCells)
					{
						NewDeactivationSet.Add(Cell);
					}

					if (!bIsFirstUpdate)
					{
						TArray<FIntVector> OldActivationCells;
						TArray<FIntVector> OldDeactivationCells;
						MeshGrid.GetCellsInRadius(OldCell, ActivationRadiusCells, OldActivationCells);
						MeshGrid.GetCellsInRadius(OldCell, DeactivationRadiusCells, OldDeactivationCells);

						// Build OldDeactivation set
						OldDeactivationSet.Reserve(OldDeactivationCells.Num());
						for (const FIntVector& Cell : OldDeactivationCells)
						{
							OldDeactivationSet.Add(Cell);
						}

						// Deactivated: cells in OldDeactivation but NOT in NewDeactivation
						TArray<FMassEntityHandle> DeactivatedEntities;
						for (const FIntVector& Cell : OldDeactivationCells)
						{
							if (!NewDeactivationSet.Contains(Cell))
							{
								if (const TArray<FMassEntityHandle>* Entities = MeshGrid.GetEntitiesInCell(Cell))
								{
									DeactivatedEntities.Append(*Entities);
								}
							}
						}

						if (DeactivatedEntities.Num() > 0)
						{
							UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
							if (SignalSubsystem)
							{
								SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisMeshDeactivated, DeactivatedEntities);
							}
						}
					}

					// Activated: cells in NewActivation but NOT in OldDeactivation
					// (on first update, OldDeactivationSet is empty so all NewActivation cells activate)
					TArray<FMassEntityHandle> ActivatedEntities;
					for (const FIntVector& Cell : NewActivationCells)
					{
						if (!OldDeactivationSet.Contains(Cell))
						{
							if (const TArray<FMassEntityHandle>* Entities = MeshGrid.GetEntitiesInCell(Cell))
							{
								ActivatedEntities.Append(*Entities);
							}
						}
					}

					UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
					if (SignalSubsystem)
					{
						if (ActivatedEntities.Num() > 0)
						{
							SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisMeshActivated, ActivatedEntities);
						}
					}

					Subsystem->UpdateMeshPlayerCell(NewCell);
				}

				// ---- Physics grid tracking ----
				{
					const FIntVector NewPhysicsCell = PhysicsGrid.WorldToCell(PlayerLocation);
					const FIntVector OldPhysicsCell = Subsystem->GetLastPhysicsPlayerCell();

					if (NewPhysicsCell != OldPhysicsCell)
					{
						const int32 PhysActivationRadiusCells = Subsystem->GetPhysicsActivationRadiusCells();
						const int32 PhysDeactivationRadiusCells = Subsystem->GetPhysicsDeactivationRadiusCells();
						const FIntVector SentinelCell = FIntVector(TNumericLimits<int32>::Max());
						const bool bIsFirstPhysicsUpdate = (OldPhysicsCell == SentinelCell);

						TArray<FIntVector> NewPhysActivationCells;
						TArray<FIntVector> NewPhysDeactivationCells;
						PhysicsGrid.GetCellsInRadius(NewPhysicsCell, PhysActivationRadiusCells, NewPhysActivationCells);
						PhysicsGrid.GetCellsInRadius(NewPhysicsCell, PhysDeactivationRadiusCells, NewPhysDeactivationCells);

						TSet<FIntVector> OldPhysDeactivationSet;
						TSet<FIntVector> NewPhysDeactivationSet;

						NewPhysDeactivationSet.Reserve(NewPhysDeactivationCells.Num());
						for (const FIntVector& Cell : NewPhysDeactivationCells)
						{
							NewPhysDeactivationSet.Add(Cell);
						}

						if (!bIsFirstPhysicsUpdate)
						{
							TArray<FIntVector> OldPhysActivationCells;
							TArray<FIntVector> OldPhysDeactivationCells;
							PhysicsGrid.GetCellsInRadius(OldPhysicsCell, PhysActivationRadiusCells, OldPhysActivationCells);
							PhysicsGrid.GetCellsInRadius(OldPhysicsCell, PhysDeactivationRadiusCells, OldPhysDeactivationCells);

							OldPhysDeactivationSet.Reserve(OldPhysDeactivationCells.Num());
							for (const FIntVector& Cell : OldPhysDeactivationCells)
							{
								OldPhysDeactivationSet.Add(Cell);
							}

							TArray<FMassEntityHandle> PhysDeactivatedEntities;
							for (const FIntVector& Cell : OldPhysDeactivationCells)
							{
								if (!NewPhysDeactivationSet.Contains(Cell))
								{
									if (const TArray<FMassEntityHandle>* Entities = PhysicsGrid.GetEntitiesInCell(Cell))
									{
										PhysDeactivatedEntities.Append(*Entities);
									}
								}
							}

							if (PhysDeactivatedEntities.Num() > 0)
							{
								UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
								if (SignalSubsystem)
								{
									SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisPhysicsDeactivated, PhysDeactivatedEntities);
								}
							}
						}

						TArray<FMassEntityHandle> PhysActivatedEntities;
						for (const FIntVector& Cell : NewPhysActivationCells)
						{
							if (!OldPhysDeactivationSet.Contains(Cell))
							{
								if (const TArray<FMassEntityHandle>* Entities = PhysicsGrid.GetEntitiesInCell(Cell))
								{
									PhysActivatedEntities.Append(*Entities);
								}
							}
						}

						UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
						if (SignalSubsystem)
						{
							if (PhysActivatedEntities.Num() > 0)
							{
								SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisPhysicsActivated, PhysActivatedEntities);
							}
						}

						Subsystem->UpdatePhysicsPlayerCell(NewPhysicsCell);
					}
				}

#if WITH_GAMEPLAY_DEBUGGER
				extern TAutoConsoleVariable<bool> CVarArcDebugDrawVisualizationGrid;
				if (CVarArcDebugDrawVisualizationGrid.GetValueOnAnyThread())
				{
					const float CellSize = MeshGrid.CellSize;
					const float HalfCell = CellSize * 0.5f;
					const FIntVector CurrentPlayerCell = Subsystem->GetLastMeshPlayerCell();

					// Draw activation radius sphere (white) and deactivation radius sphere (orange)
					DrawDebugSphere(World, PlayerLocation, Subsystem->GetMeshActivationRadius(), 32, FColor::White, false, -1.f, 0, 1.f);
					DrawDebugSphere(World, PlayerLocation, Subsystem->GetMeshDeactivationRadius(), 32, FColor::Orange, false, -1.f, 0, 1.f);

					// Draw all cells that contain entities, colored by state:
					//   Green  = Has mesh rendering active
					//   Blue   = No mesh rendering (outside activation range)
					for (const TPair<FIntVector, TArray<FMassEntityHandle>>& CellPair : MeshGrid.CellEntities)
					{
						const FIntVector& CellCoord = CellPair.Key;
						const TArray<FMassEntityHandle>& Entities = CellPair.Value;

						const FVector CellCenter(
							CellCoord.X * CellSize + HalfCell,
							CellCoord.Y * CellSize + HalfCell,
							CellCoord.Z * CellSize + HalfCell
						);

						// Determine cell state from first valid entity
						bool bHasMeshRendering = false;
						for (const FMassEntityHandle& Entity : Entities)
						{
							if (EntityManager.IsEntityValid(Entity))
							{
								const FArcVisRepresentationFragment* Rep = EntityManager.GetFragmentDataPtr<
									FArcVisRepresentationFragment>(Entity);
								if (Rep)
								{
									bHasMeshRendering = Rep->bHasMeshRendering;
									break;
								}
							}
						}

						const FColor DrawColor = bHasMeshRendering ? FColor::Green : FColor::Blue;
						DrawDebugBox(World, CellCenter, FVector(HalfCell * 0.9f), DrawColor, false, -1.f, 0, 2.f);

						// Draw entity count label
						const FString CountLabel = FString::Printf(TEXT("%d %s"), Entities.Num(), bHasMeshRendering ?
							TEXT("MESH") :
							TEXT("ACT"));
						DrawDebugString(World, CellCenter + FVector(0, 0, HalfCell * 0.7f), CountLabel, nullptr, bHasMeshRendering ?
							FColor::Green :
							FColor::Cyan, -1.f, true, 1.f);
					}

					// Draw active cell boundary (cells within activation radius on the edge)
					const int32 ActivationCells = Subsystem->GetMeshActivationRadiusCells();
					TArray<FIntVector> ActiveCells;
					MeshGrid.GetCellsInRadius(CurrentPlayerCell, ActivationCells, ActiveCells);

					for (const FIntVector& Cell : ActiveCells)
					{
						if (!MeshGrid.GetEntitiesInCell(Cell))
						{
							continue;
						}

						const int32 DX = FMath::Abs(Cell.X - CurrentPlayerCell.X);
						const int32 DY = FMath::Abs(Cell.Y - CurrentPlayerCell.Y);
						const int32 DZ = FMath::Abs(Cell.Z - CurrentPlayerCell.Z);
						if (DX == ActivationCells || DY == ActivationCells || DZ == ActivationCells)
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

					// ---- Physics grid debug draw ----
					// Activation (yellow) and deactivation (red) radius spheres
					DrawDebugSphere(World, PlayerLocation, Subsystem->GetPhysicsActivationRadius(), 32, FColor::Yellow, false, -1.f, 0, 1.f);
					DrawDebugSphere(World, PlayerLocation, Subsystem->GetPhysicsDeactivationRadius(), 32, FColor::Red, false, -1.f, 0, 1.f);

					const float PhysCellSize = PhysicsGrid.CellSize;
					const float PhysHalfCell = PhysCellSize * 0.5f;
					const FIntVector CurrentPhysicsPlayerCell = Subsystem->GetLastPhysicsPlayerCell();

					// Draw physics grid cells: purple = has physics body active, yellow = no physics body
					for (const TPair<FIntVector, TArray<FMassEntityHandle>>& PhysCellPair : PhysicsGrid.CellEntities)
					{
						const FIntVector& PhysCellCoord = PhysCellPair.Key;
						const FVector PhysCellCenter(
							PhysCellCoord.X * PhysCellSize + PhysHalfCell,
							PhysCellCoord.Y * PhysCellSize + PhysHalfCell,
							PhysCellCoord.Z * PhysCellSize + PhysHalfCell
						);

						const bool bIsActivePhysicsCell = Subsystem->IsPhysicsActiveCellCoord(PhysCellCoord);
						const FColor PhysDrawColor = bIsActivePhysicsCell ? FColor::Purple : FColor::Yellow;
						DrawDebugBox(World, PhysCellCenter, FVector(PhysHalfCell * 0.85f), PhysDrawColor, false, -1.f, 0, 1.5f);

						const FString PhysCountLabel = FString::Printf(TEXT("%d %s"), PhysCellPair.Value.Num(),
							bIsActivePhysicsCell ? TEXT("PHYS") : TEXT("OFF"));
						DrawDebugString(World, PhysCellCenter + FVector(0, 0, PhysHalfCell * 0.5f), PhysCountLabel, nullptr,
							bIsActivePhysicsCell ? FColor::Purple : FColor::Yellow, -1.f, true, 0.8f);
					}

					// Draw physics player cell highlight
					const FIntVector PhysSentinelCell = FIntVector(TNumericLimits<int32>::Max());
					if (CurrentPhysicsPlayerCell != PhysSentinelCell)
					{
						const FVector PhysPlayerCellCenter(
							CurrentPhysicsPlayerCell.X * PhysCellSize + PhysHalfCell,
							CurrentPhysicsPlayerCell.Y * PhysCellSize + PhysHalfCell,
							CurrentPhysicsPlayerCell.Z * PhysCellSize + PhysHalfCell
						);
						DrawDebugBox(World, PhysPlayerCellCenter, FVector(PhysHalfCell * 0.9f), FColor::Yellow, false, -1.f, 0, 3.f);
					}
				}
#endif
			}
		}
	);
}

// ---------------------------------------------------------------------------
// UArcVisMeshActivateProcessor
// ---------------------------------------------------------------------------

UArcVisMeshActivateProcessor::UArcVisMeshActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisMeshActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshActivated);
}

void UArcVisMeshActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FArcVisLifecycleFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcVisLifecycleConfigFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcVisPrePlacedActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcVisComponentTransformFragment>(EMassFragmentPresence::Optional);
}

void UArcVisMeshActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
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

	const bool bEnableActorSwapping = GetDefault<UArcVisSettings>()->bEnableActorSwapping;

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisMeshActivate);

	TArray<FArcPendingISMActivation> PendingActivations;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, bEnableActorSwapping, &PendingActivations](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
			const FArcMassStaticMeshConfigFragment& StaticMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassStaticMeshConfigFragment>();
			const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassVisualizationMeshConfigFragment>();
			const FMassOverrideMaterialsFragment* OverrideMatsPtr = Ctx.GetConstSharedFragmentPtr<FMassOverrideMaterialsFragment>();

			const FArcVisLifecycleConfigFragment* LifecycleConfig = Ctx.GetConstSharedFragmentPtr<FArcVisLifecycleConfigFragment>();
			TConstArrayView<FArcVisLifecycleFragment> LifecycleFrags = Ctx.GetFragmentView<FArcVisLifecycleFragment>();
			TConstArrayView<FArcVisPrePlacedActorFragment> PrePlacedFrags = Ctx.GetFragmentView<FArcVisPrePlacedActorFragment>();
			const FArcVisComponentTransformFragment* CompTransformPtr = Ctx.GetConstSharedFragmentPtr<FArcVisComponentTransformFragment>();

			const bool bHasLifecycle = LifecycleFrags.Num() > 0;
			const bool bHasPrePlaced = PrePlacedFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (Rep.bHasMeshRendering)
				{
					continue;
				}

				const FTransformFragment& TransformFrag = Transforms[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Actor swapping — check if this entity should use actor representation
				if (bEnableActorSwapping && Config.ActorClass)
				{
					if (bHasPrePlaced)
					{
						const FArcVisPrePlacedActorFragment& PrePlaced = PrePlacedFrags[EntityIt];
						AActor* PrePlacedActor = PrePlaced.PrePlacedActor.Get();
						if (PrePlacedActor)
						{
							PrePlacedActor->SetActorHiddenInGame(false);
							PrePlacedActor->SetActorEnableCollision(true);
							FMassActorFragment& ActorFrag = ActorFrags[EntityIt];
							ActorFrag.SetAndUpdateHandleMap(Entity, PrePlacedActor, /*bIsOwnedByMass=*/false);
							Rep.bIsActorRepresentation = true;
							Rep.bHasMeshRendering = true;

							UArcVisEntityComponent* VisComp = PrePlacedActor->FindComponentByClass<UArcVisEntityComponent>();
							if (VisComp)
							{
								VisComp->NotifyVisActorCreated(Entity);
							}
							continue;
						}
					}

					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					AActor* SpawnedActor = World->SpawnActor<AActor>(Config.ActorClass, TransformFrag.GetTransform(), SpawnParams);
					if (SpawnedActor)
					{
						FMassActorFragment& ActorFrag = ActorFrags[EntityIt];
						ActorFrag.SetAndUpdateHandleMap(Entity, SpawnedActor, /*bIsOwnedByMass=*/true);
						Rep.bIsActorRepresentation = true;
						Rep.bHasMeshRendering = true;

						UArcVisEntityComponent* VisComp = SpawnedActor->FindComponentByClass<UArcVisEntityComponent>();
						if (VisComp)
						{
							VisComp->NotifyVisActorCreated(Entity);
						}
					}
					continue;
				}

				// ISM path — resolve mesh (lifecycle override if applicable)
				const FArcMassStaticMeshConfigFragment* ResolvedMeshFrag = &StaticMeshConfigFrag;
				FArcMassStaticMeshConfigFragment LifecycleMeshFrag;
				if (bHasLifecycle && LifecycleConfig)
				{
					const FArcVisLifecycleFragment& LifecycleFrag = LifecycleFrags[EntityIt];
					UStaticMesh* PhaseMesh = LifecycleConfig->ResolveMesh(LifecycleFrag.CurrentPhase, StaticMeshConfigFrag);
					if (PhaseMesh && PhaseMesh != StaticMeshConfigFrag.Mesh.Get())
					{
						LifecycleMeshFrag = FArcMassStaticMeshConfigFragment(PhaseMesh);
						ResolvedMeshFrag = &LifecycleMeshFrag;
					}
				}

				// Collect into pending batch — resolved in deferred command
				FArcPendingISMActivation Activation;
				Activation.SourceEntity = Entity;
				Activation.Cell = Rep.MeshGridCoords;
				Activation.WorldTransform = CompTransformPtr
					? CompTransformPtr->ComponentRelativeTransform * TransformFrag.GetTransform()
					: TransformFrag.GetTransform();
				Activation.StaticMeshConfigFrag = *ResolvedMeshFrag;
				Activation.VisMeshConfigFrag = VisMeshConfigFrag;
				if (OverrideMatsPtr)
				{
					Activation.OverrideMats = *OverrideMatsPtr;
					Activation.bHasOverrideMats = true;
				}
				PendingActivations.Add(MoveTemp(Activation));

				Rep.bHasMeshRendering = true;
			}
		});

	Subsystem->BatchActivateISMEntities(MoveTemp(PendingActivations), Context);
}

// ---------------------------------------------------------------------------
// UArcVisMeshDeactivateProcessor
// ---------------------------------------------------------------------------

UArcVisMeshDeactivateProcessor::UArcVisMeshDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisMeshDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshDeactivated);
}

void UArcVisMeshDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddRequirement<FArcVisPrePlacedActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UArcVisMeshDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisMeshDeactivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, &Context](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
			TConstArrayView<FArcVisISMInstanceFragment> ISMInstanceFrags = Ctx.GetFragmentView<FArcVisISMInstanceFragment>();
			TConstArrayView<FArcVisPrePlacedActorFragment> PrePlacedFrags = Ctx.GetFragmentView<FArcVisPrePlacedActorFragment>();

			const bool bHasISMInstance = ISMInstanceFrags.Num() > 0;
			const bool bHasPrePlaced = PrePlacedFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (!Rep.bHasMeshRendering)
				{
					continue;
				}

				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// ISM instance cleanup
				if (bHasISMInstance)
				{
					const FArcVisISMInstanceFragment& ISMInstance = ISMInstanceFrags[EntityIt];
					if (ISMInstance.InstanceIndex != INDEX_NONE)
					{
						Subsystem->RemoveISMInstance(
							ISMInstance.HolderEntity, ISMInstance.InstanceIndex, Rep.MeshGridCoords, EntityManager);
					}
					Context.Defer().RemoveFragment<FArcVisISMInstanceFragment>(Entity);
				}

				// Actor cleanup
				if (Rep.bIsActorRepresentation)
				{
					FMassActorFragment& ActorFrag = ActorFrags[EntityIt];
					AActor* Actor = ActorFrag.GetMutable();

					if (Actor)
					{
						bool bIsPrePlaced = false;
						if (bHasPrePlaced)
						{
							const FArcVisPrePlacedActorFragment& PrePlaced = PrePlacedFrags[EntityIt];
							bIsPrePlaced = (PrePlaced.PrePlacedActor.Get() == Actor);
						}

						if (bIsPrePlaced)
						{
							Actor->SetActorHiddenInGame(true);
							Actor->SetActorEnableCollision(false);
						}
						else if (ActorFrag.IsOwnedByMass())
						{
							Actor->Destroy();
						}
						ActorFrag.ResetAndUpdateHandleMap();
					}
					Rep.bIsActorRepresentation = false;
				}

				Rep.bHasMeshRendering = false;
			}
		});
}

// ---------------------------------------------------------------------------
// UArcVisPhysicsActivateProcessor
// ---------------------------------------------------------------------------

UArcVisPhysicsActivateProcessor::UArcVisPhysicsActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisPhysicsActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisPhysicsActivated);
}

void UArcVisPhysicsActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
}

void UArcVisPhysicsActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	FPhysScene* PhysScene = World->GetPhysicsScene();
	if (!PhysScene)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisPhysicsActivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, PhysScene, &Context](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
			const FArcMassPhysicsBodyConfigFragment& PhysicsConfig = Ctx.GetConstSharedFragment<FArcMassPhysicsBodyConfigFragment>();

			if (!Config.bEnablePhysicsBody || !PhysicsConfig.BodySetup)
			{
				return;
			}

			TArray<FMassEntityHandle> PendingEntities;
			TArray<FTransform> PendingTransforms;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				if (Rep.bHasPhysicsBody)
				{
					continue;
				}

				PendingEntities.Add(Ctx.GetEntity(EntityIt));
				PendingTransforms.Add(Transforms[EntityIt].GetTransform());
			}

			if (PendingEntities.Num() == 0)
			{
				return;
			}

			TArray<FBodyInstance*> Bodies;
			UE::ArcMass::Physics::InitBodiesFromConfig(PhysicsConfig, PendingTransforms, PhysScene, Bodies);

			for (int32 Index = 0; Index < PendingEntities.Num(); ++Index)
			{
				FBodyInstance* Body = Bodies.IsValidIndex(Index) ? Bodies[Index] : nullptr;
				if (!Body)
				{
					continue;
				}

				FMassEntityHandle Entity = PendingEntities[Index];

				// Attach entity link to Chaos particle before deferring fragment storage
				if (Body->GetPhysicsActor())
				{
					ArcMassPhysicsEntityLink::Attach(*Body, Entity, nullptr);
				}

				Context.Defer().PushCommand<FMassDeferredCreateCommand>(
					[Entity, Body](FMassEntityManager& DeferredEM)
					{
						if (!DeferredEM.IsEntityValid(Entity))
						{
							Body->TermBody();
							delete Body;
							return;
						}

						FArcMassPhysicsBodyFragment PhysicsBodyFrag;
						PhysicsBodyFrag.Body = Body;
						DeferredEM.AddFragmentToEntity(Entity, FArcMassPhysicsBodyFragment::StaticStruct(),
							[&PhysicsBodyFrag](void* Dst, const UScriptStruct& DstType)
							{
								FArcMassPhysicsBodyFragment* DstFrag = static_cast<FArcMassPhysicsBodyFragment*>(Dst);
								DstFrag->Body = PhysicsBodyFrag.Body;
							});

						FArcVisRepresentationFragment* RepPtr = DeferredEM.GetFragmentDataPtr<FArcVisRepresentationFragment>(Entity);
						if (RepPtr)
						{
							RepPtr->bHasPhysicsBody = true;
						}

					});
			}
		});
}

// ---------------------------------------------------------------------------
// UArcVisPhysicsDeactivateProcessor
// ---------------------------------------------------------------------------

UArcVisPhysicsDeactivateProcessor::UArcVisPhysicsDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcVisPhysicsDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisPhysicsDeactivated);
}

void UArcVisPhysicsDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::All);
}

void UArcVisPhysicsDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisPhysicsDeactivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&Context](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FArcMassPhysicsBodyFragment> PhysicsBodyFrags = Ctx.GetMutableFragmentView<FArcMassPhysicsBodyFragment>();

			TArray<FBodyInstance::FAsyncTermBodyPayload> Payloads;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (!Rep.bHasPhysicsBody)
				{
					continue;
				}

				FArcMassPhysicsBodyFragment& PhysicsBody = PhysicsBodyFrags[EntityIt];
				if (PhysicsBody.Body)
				{
					FBodyInstance::FAsyncTermBodyPayload Payload = PhysicsBody.Body->StartAsyncTermBody_GameThread();
					if (Payload.GetPhysicsActor())
					{
						Payloads.Add(MoveTemp(Payload));
					}
					delete PhysicsBody.Body;
					PhysicsBody.Body = nullptr;
				}

				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				Context.Defer().RemoveFragment<FArcMassPhysicsBodyFragment>(Entity);

				Rep.bHasPhysicsBody = false;
			}

			UE::ArcMass::Physics::AsyncTermBodies(MoveTemp(Payloads));
		});
}

// ---------------------------------------------------------------------------
// UArcVisEntityInitObserver
// ---------------------------------------------------------------------------

UArcVisEntityInitObserver::UArcVisEntityInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcVisEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcVisEntityInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FMassOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
	ObserverQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FArcVisPrePlacedActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::Optional);
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

	const bool bEnableActorSwapping = GetDefault<UArcVisSettings>()->bEnableActorSwapping;
	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisEntityInit);

	TArray<FMassEntityHandle> ActivateEntities;
	TArray<FMassEntityHandle> PhysicsActivateEntities;

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, bEnableActorSwapping, &ActivateEntities, &PhysicsActivateEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();

			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
			TConstArrayView<FArcVisPrePlacedActorFragment> PrePlacedFrags = Ctx.GetFragmentView<FArcVisPrePlacedActorFragment>();
			const bool bHasPrePlaced = PrePlacedFrags.Num() > 0;
			const FArcVisualizationGrid& MeshGrid = Subsystem->GetMeshGrid();

			const FArcMassPhysicsBodyConfigFragment* PhysicsConfigPtr = Ctx.GetConstSharedFragmentPtr<FArcMassPhysicsBodyConfigFragment>();
			const bool bHasPhysicsConfig = PhysicsConfigPtr && PhysicsConfigPtr->BodySetup && Config.bEnablePhysicsBody;
			const FArcVisualizationGrid& PhysicsGrid = Subsystem->GetPhysicsGrid();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				const FTransformFragment& TransformFrag = Transforms[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// 1. Register in mesh grid
				const FVector WorldPos = TransformFrag.GetTransform().GetLocation();
				Rep.MeshGridCoords = MeshGrid.WorldToCell(WorldPos);
				Subsystem->RegisterMeshEntity(Entity, WorldPos);

				// 2. Register in physics grid (if entity has physics config)
				if (bHasPhysicsConfig)
				{
					Rep.PhysicsGridCoords = PhysicsGrid.WorldToCell(WorldPos);
					Subsystem->RegisterPhysicsEntity(Entity, WorldPos);
				}

				// 3. Handle pre-placed actors
				if (bHasPrePlaced)
				{
					const FArcVisPrePlacedActorFragment& PrePlaced = PrePlacedFrags[EntityIt];
					AActor* PrePlacedActor = PrePlaced.PrePlacedActor.Get();
					if (PrePlacedActor)
					{
						if (bEnableActorSwapping && Config.ActorClass)
						{
							// Actor-tier enabled: hide for now, will be shown on activate
							PrePlacedActor->SetActorHiddenInGame(true);
							PrePlacedActor->SetActorEnableCollision(false);
						}
						else
						{
							// Actor-tier disabled: destroy immediately
							PrePlacedActor->Destroy();
						}
					}
				}

				// 4. If in active mesh cell, queue for activation
				if (Subsystem->IsMeshActiveCellCoord(Rep.MeshGridCoords))
				{
					ActivateEntities.Add(Entity);
				}

				// 5. If in active physics cell, queue for physics activation
				if (bHasPhysicsConfig && Subsystem->IsPhysicsActiveCellCoord(Rep.PhysicsGridCoords))
				{
					PhysicsActivateEntities.Add(Entity);
				}
			}
		});

	// Signal activate for entities in active cells
	if (SignalSubsystem && ActivateEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisMeshActivated, ActivateEntities);
	}

	if (SignalSubsystem && PhysicsActivateEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisPhysicsActivated, PhysicsActivateEntities);
	}
}

// ---------------------------------------------------------------------------
// UArcVisEntityDeinitObserver
// ---------------------------------------------------------------------------

UArcVisEntityDeinitObserver::UArcVisEntityDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcVisEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcVisEntityDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcVisRepresentationFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FArcVisPrePlacedActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisEntityDeinit);

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TArrayView<FMassActorFragment> ActorFrags = Ctx.GetMutableFragmentView<FMassActorFragment>();
			TConstArrayView<FArcVisISMInstanceFragment> ISMInstanceFrags = Ctx.GetFragmentView<FArcVisISMInstanceFragment>();
			TConstArrayView<FArcVisPrePlacedActorFragment> PrePlacedFrags = Ctx.GetFragmentView<FArcVisPrePlacedActorFragment>();

			const FArcMassPhysicsBodyConfigFragment* PhysicsConfigPtr = Ctx.GetConstSharedFragmentPtr<FArcMassPhysicsBodyConfigFragment>();
			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
			const bool bHasPhysicsConfig = PhysicsConfigPtr && PhysicsConfigPtr->BodySetup && Config.bEnablePhysicsBody;
			TArrayView<FArcMassPhysicsBodyFragment> PhysicsBodyFrags = Ctx.GetMutableFragmentView<FArcMassPhysicsBodyFragment>();
			const bool bHasPhysicsBody = PhysicsBodyFrags.Num() > 0;

			const bool bHasISMInstance = ISMInstanceFrags.Num() > 0;
			const bool bHasPrePlaced = PrePlacedFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// 1. ISM cleanup
				if (Rep.bHasMeshRendering && bHasISMInstance)
				{
					const FArcVisISMInstanceFragment& ISMInstance = ISMInstanceFrags[EntityIt];
					if (ISMInstance.InstanceIndex != INDEX_NONE)
					{
						Subsystem->RemoveISMInstance(
							ISMInstance.HolderEntity, ISMInstance.InstanceIndex, Rep.MeshGridCoords, EntityManager);
					}
				}

				// 2. Actor cleanup
				if (Rep.bIsActorRepresentation)
				{
					FMassActorFragment& ActorFrag = ActorFrags[EntityIt];
					AActor* Actor = ActorFrag.GetMutable();
					if (Actor && ActorFrag.IsOwnedByMass())
					{
						Actor->Destroy();
					}
					ActorFrag.ResetAndUpdateHandleMap();
				}

				// 3. Unregister from mesh grid
				Subsystem->UnregisterMeshEntity(Entity, Rep.MeshGridCoords);

				// 4. Unregister from physics grid and terminate physics body
				if (bHasPhysicsConfig)
				{
					Subsystem->UnregisterPhysicsEntity(Entity, Rep.PhysicsGridCoords);
				}

				if (bHasPhysicsBody)
				{
					FArcMassPhysicsBodyFragment& PhysicsBody = PhysicsBodyFrags[EntityIt];
					PhysicsBody.TerminateBody();
				}
			}
		});
}
