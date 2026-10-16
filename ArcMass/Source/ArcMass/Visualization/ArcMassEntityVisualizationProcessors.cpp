// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassEntityVisualizationProcessors.h"

#include "ArcMassEntityVisualization.h"
#include "ArcMassRenderLogs.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcMass/Visualization/ArcLifecyclePhaseVisualsFragment.h"
#include "DrawDebugHelpers.h"
#include "MassCommonFragments.h"
#include "Mass/Commands.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Engine/StaticMesh.h"

// ArcMassRender mesh rendering
#include "Mesh/ArcRenderMeshFragments.h"
#include "ArcMassVisualizationConfigFragments.h"

// Composite visualization — tag used to explicitly exclude composite entities from single-mesh queries
#include "ArcMass/CompositeVisualization/ArcCompositeMeshVisualizationFragments.h"

// Physics
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "ArcVisProcessorUtils.h"
// ---------------------------------------------------------------------------
// UArcVisPlayerCellTrackingProcessor
// ---------------------------------------------------------------------------

UArcVisPlayerCellTrackingProcessor::UArcVisPlayerCellTrackingProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
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
		[this, &EntityManager, Subsystem, World, &MeshGrid, &PhysicsGrid] (FMassExecutionContext& Ctx)
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
					ScratchNewActivationCells.Reset();
					ScratchNewDeactivationCells.Reset();
					MeshGrid.GetCellsInRadius(NewCell, ActivationRadiusCells, ScratchNewActivationCells);
					MeshGrid.GetCellsInRadius(NewCell, DeactivationRadiusCells, ScratchNewDeactivationCells);

					ScratchOldDeactivationSet.Reset();
					ScratchNewDeactivationSet.Reset();

					// Build NewDeactivation set
					ScratchNewDeactivationSet.Reserve(ScratchNewDeactivationCells.Num());
					for (const FIntVector& Cell : ScratchNewDeactivationCells)
					{
						ScratchNewDeactivationSet.Add(Cell);
					}

					if (!bIsFirstUpdate)
					{
						ScratchOldActivationCells.Reset();
						ScratchOldDeactivationCells.Reset();
						MeshGrid.GetCellsInRadius(OldCell, ActivationRadiusCells, ScratchOldActivationCells);
						MeshGrid.GetCellsInRadius(OldCell, DeactivationRadiusCells, ScratchOldDeactivationCells);

						// Build OldDeactivation set
						ScratchOldDeactivationSet.Reserve(ScratchOldDeactivationCells.Num());
						for (const FIntVector& Cell : ScratchOldDeactivationCells)
						{
							ScratchOldDeactivationSet.Add(Cell);
						}

						// Deactivated: cells in OldDeactivation but NOT in NewDeactivation
						ScratchDeactivatedEntities.Reset();
						for (const FIntVector& Cell : ScratchOldDeactivationCells)
						{
							if (!ScratchNewDeactivationSet.Contains(Cell))
							{
								if (const TArray<FMassEntityHandle>* Entities = MeshGrid.GetEntitiesInCell(Cell))
								{
									ScratchDeactivatedEntities.Append(*Entities);
								}
							}
						}

						if (ScratchDeactivatedEntities.Num() > 0)
						{
							UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
							if (SignalSubsystem)
							{
								SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisMeshDeactivated, ScratchDeactivatedEntities);
							}
						}
					}

					// Activated: cells in NewActivation but NOT in OldDeactivation
					// (on first update, OldDeactivationSet is empty so all NewActivation cells activate)
					ScratchActivatedEntities.Reset();
					for (const FIntVector& Cell : ScratchNewActivationCells)
					{
						if (!ScratchOldDeactivationSet.Contains(Cell))
						{
							if (const TArray<FMassEntityHandle>* Entities = MeshGrid.GetEntitiesInCell(Cell))
							{
								ScratchActivatedEntities.Append(*Entities);
							}
						}
					}

					UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
					if (SignalSubsystem)
					{
						if (ScratchActivatedEntities.Num() > 0)
						{
							SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisMeshActivated, ScratchActivatedEntities);
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

						ScratchNewPhysActivationCells.Reset();
						ScratchNewPhysDeactivationCells.Reset();
						PhysicsGrid.GetCellsInRadius(NewPhysicsCell, PhysActivationRadiusCells, ScratchNewPhysActivationCells);
						PhysicsGrid.GetCellsInRadius(NewPhysicsCell, PhysDeactivationRadiusCells, ScratchNewPhysDeactivationCells);

						ScratchOldPhysDeactivationSet.Reset();
						ScratchNewPhysDeactivationSet.Reset();

						ScratchNewPhysDeactivationSet.Reserve(ScratchNewPhysDeactivationCells.Num());
						for (const FIntVector& Cell : ScratchNewPhysDeactivationCells)
						{
							ScratchNewPhysDeactivationSet.Add(Cell);
						}

						if (!bIsFirstPhysicsUpdate)
						{
							ScratchOldPhysActivationCells.Reset();
							ScratchOldPhysDeactivationCells.Reset();
							PhysicsGrid.GetCellsInRadius(OldPhysicsCell, PhysActivationRadiusCells, ScratchOldPhysActivationCells);
							PhysicsGrid.GetCellsInRadius(OldPhysicsCell, PhysDeactivationRadiusCells, ScratchOldPhysDeactivationCells);

							ScratchOldPhysDeactivationSet.Reserve(ScratchOldPhysDeactivationCells.Num());
							for (const FIntVector& Cell : ScratchOldPhysDeactivationCells)
							{
								ScratchOldPhysDeactivationSet.Add(Cell);
							}

							ScratchPhysDeactivatedEntities.Reset();
							for (const FIntVector& Cell : ScratchOldPhysDeactivationCells)
							{
								if (!ScratchNewPhysDeactivationSet.Contains(Cell))
								{
									if (const TArray<FMassEntityHandle>* Entities = PhysicsGrid.GetEntitiesInCell(Cell))
									{
										ScratchPhysDeactivatedEntities.Append(*Entities);
									}
								}
							}

							if (ScratchPhysDeactivatedEntities.Num() > 0)
							{
								UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
								if (SignalSubsystem)
								{
									SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyReleased, ScratchPhysDeactivatedEntities);
								}
							}
						}

						ScratchPhysActivatedEntities.Reset();
						for (const FIntVector& Cell : ScratchNewPhysActivationCells)
						{
							if (!ScratchOldPhysDeactivationSet.Contains(Cell))
							{
								if (const TArray<FMassEntityHandle>* Entities = PhysicsGrid.GetEntitiesInCell(Cell))
								{
									ScratchPhysActivatedEntities.Append(*Entities);
								}
							}
						}

						UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
						if (SignalSubsystem)
						{
							if (ScratchPhysActivatedEntities.Num() > 0)
							{
								SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, ScratchPhysActivatedEntities);
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
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
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
	EntityQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcCompositeMeshVisEntityTag>(EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcLifecyclePhaseVisualsFragment>(EMassFragmentPresence::Optional);
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisMeshActivate);

	TArray<FArcPendingISMActivation> PendingActivations;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, &PendingActivations](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();
			const FArcMassStaticMeshConfigFragment& StaticMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassStaticMeshConfigFragment>();
			const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassVisualizationMeshConfigFragment>();
			const FArcOverrideMaterialsFragment* OverrideMatsPtr = Ctx.GetConstSharedFragmentPtr<FArcOverrideMaterialsFragment>();

			const FArcLifecyclePhaseVisualsFragment* LifecycleVisualsConfig = Ctx.GetConstSharedFragmentPtr<FArcLifecyclePhaseVisualsFragment>();
			TConstArrayView<FArcLifecycleFragment> LifecycleFrags = Ctx.GetFragmentView<FArcLifecycleFragment>();
			const FArcVisComponentTransformFragment* CompTransformPtr = Ctx.GetConstSharedFragmentPtr<FArcVisComponentTransformFragment>();

			const bool bHasLifecycle = LifecycleFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (Rep.bHasMeshRendering)
				{
					continue;
				}

				const FTransformFragment& TransformFrag = Transforms[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// ISM path — resolve mesh (lifecycle phase override if applicable)
				const FArcMassStaticMeshConfigFragment* ResolvedMeshFrag = &StaticMeshConfigFrag;
				FArcMassStaticMeshConfigFragment LifecycleMeshFrag;
				if (bHasLifecycle && LifecycleVisualsConfig)
				{
					const FArcLifecycleFragment& LifecycleFrag = LifecycleFrags[EntityIt];
					UStaticMesh* PhaseMesh = LifecycleVisualsConfig->ResolveMesh(LifecycleFrag.CurrentPhase, StaticMeshConfigFrag);
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
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
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
	EntityQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcCompositeMeshVisEntityTag>(EMassFragmentPresence::None);
	EntityQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	// Static-mesh discriminator (the activate processor already has it). Without it this processor
	// matched SKINNED entities on the shared VisMeshDeactivated signal: it cleared bHasMeshRendering
	// and deferred the ISM-instance fragment removal, so if it ran before the skinned deactivate
	// processor, the skinned one skipped the entity and its manager instance leaked.
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::All);
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
			TConstArrayView<FArcVisISMInstanceFragment> ISMInstanceFrags = Ctx.GetFragmentView<FArcVisISMInstanceFragment>();

			const bool bHasISMInstance = ISMInstanceFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];

				if (!Rep.bHasMeshRendering)
				{
					continue;
				}

				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// ISM instance cleanup — pass source entity + SubIndex=0 (single-mesh path always uses 0).
				if (bHasISMInstance)
				{
					const FArcVisISMInstanceFragment& ISMInstance = ISMInstanceFrags[EntityIt];
					if (ISMInstance.HolderEntity.IsValid())
					{
						Subsystem->RemoveISMInstance(
							ISMInstance.HolderEntity, Entity, 0, Rep.MeshGridCoords, EntityManager);
					}
					Context.Defer().RemoveFragment<FArcVisISMInstanceFragment>(Entity);
				}

				Rep.bHasMeshRendering = false;
			}
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
	ObserverQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
	ObserverQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
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

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisEntityInit);

	TArray<FMassEntityHandle> ActivateEntities;
	TArray<FMassEntityHandle> PhysicsActivateEntities;

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, &ActivateEntities, &PhysicsActivateEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

			const FArcVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcVisConfigFragment>();

			const FArcMassPhysicsBodyConfigFragment* PhysicsConfigPtr = Ctx.GetConstSharedFragmentPtr<FArcMassPhysicsBodyConfigFragment>();
			const bool bHasPhysicsConfig = PhysicsConfigPtr && PhysicsConfigPtr->BodySetup;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				const FTransformFragment& TransformFrag = Transforms[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				const FVector WorldPos = TransformFrag.GetTransform().GetLocation();
				ArcMass::Visualization::RegisterEntityInGrids(*Subsystem, Entity, WorldPos, Rep, bHasPhysicsConfig);

				ArcMass::Visualization::CollectActivationSignals(*Subsystem, Rep, Entity, bHasPhysicsConfig, ActivateEntities, PhysicsActivateEntities);
			}
		});

	// Signal activate for entities in active cells
	if (SignalSubsystem && ActivateEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::VisMeshActivated, ActivateEntities);
	}

	if (SignalSubsystem && PhysicsActivateEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, PhysicsActivateEntities);
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
	ObserverQuery.AddConstSharedRequirement<FArcVisConfigFragment>(EMassFragmentPresence::All);
	// Static-mesh discriminator, mirroring UArcVisEntityInitObserver — without it this observer also
	// matches SKINNED entities and mis-routes them to the static RemoveISMInstance (no-op against the
	// wrong instance pool) while duplicating the skinned deinit observer's grid/physics cleanup.
	ObserverQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcVisEntityTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FArcVisISMInstanceFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	ObserverQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::Optional);
	ObserverQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcVisEntityDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] DeinitObserver: NO world — vis entities destroyed WITHOUT ISM/grid cleanup");
		return;
	}

	UArcEntityVisualizationSubsystem* Subsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!Subsystem)
	{
		// Silent skip here leaks ISM instances/holders — the holder registries never hear about
		// these entities, so later removals against the same holders act on stale bookkeeping.
		UE_LOGF(LogArcMassVisDiag, Warning,"[ArcVisDiag] DeinitObserver: vis subsystem GONE — vis entities destroyed WITHOUT ISM/grid cleanup");
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcVisEntityDeinit);

	int32 NumCleaned = 0;

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, &NumCleaned](FMassExecutionContext& Ctx)
		{
			NumCleaned += Ctx.GetNumEntities();
			TArrayView<FArcVisRepresentationFragment> Reps = Ctx.GetMutableFragmentView<FArcVisRepresentationFragment>();
			TConstArrayView<FArcVisISMInstanceFragment> ISMInstanceFrags = Ctx.GetFragmentView<FArcVisISMInstanceFragment>();

			const FArcMassPhysicsBodyConfigFragment* PhysicsConfigPtr = Ctx.GetConstSharedFragmentPtr<FArcMassPhysicsBodyConfigFragment>();
			const bool bHasPhysicsConfig = PhysicsConfigPtr && PhysicsConfigPtr->BodySetup;
			TArrayView<FArcMassPhysicsBodyFragment> PhysicsBodyFrags = Ctx.GetMutableFragmentView<FArcMassPhysicsBodyFragment>();
			const bool bHasPhysicsBody = PhysicsBodyFrags.Num() > 0;

			const bool bHasISMInstance = ISMInstanceFrags.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcVisRepresentationFragment& Rep = Reps[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// 1. ISM cleanup — pass source entity + SubIndex=0 (single-mesh path always uses 0).
				if (Rep.bHasMeshRendering && bHasISMInstance)
				{
					const FArcVisISMInstanceFragment& ISMInstance = ISMInstanceFrags[EntityIt];
					if (ISMInstance.HolderEntity.IsValid())
					{
						UE_LOGF(LogArcMassVisDiag, Verbose,"[ArcVisDiag] DeinitObserver: entity=[%d:%d] -> RemoveISMInstance holder=[%d:%d] cachedIdx=%d",
							Entity.Index, Entity.SerialNumber,
							ISMInstance.HolderEntity.Index, ISMInstance.HolderEntity.SerialNumber,
							ISMInstance.InstanceIndex);
						Subsystem->RemoveISMInstance(
							ISMInstance.HolderEntity, Entity, 0, Rep.MeshGridCoords, EntityManager);
					}
				}

				ArcMass::Visualization::UnregisterEntityFromGrids(*Subsystem, Entity, Rep, bHasPhysicsConfig);

				if (bHasPhysicsBody)
				{
					FArcMassPhysicsBodyFragment& PhysicsBody = PhysicsBodyFrags[EntityIt];
					PhysicsBody.TerminateBody();
				}
			}
		});

	UE_LOGF(LogArcMassVisDiag, Log,"[ArcVisDiag] DeinitObserver: cleaned up %d destroyed vis entities", NumCleaned);
}
