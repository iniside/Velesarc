// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisualizationProcessors.h"

#include "ArcMobileVisualization.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
	struct FMobileVisRadii
	{
		int32 ActorRadiusCells;
		int32 ISMRadiusCells;
		int32 ActorRadiusCellsLeave;
		int32 ISMRadiusCellsLeave;
	};

	FMobileVisRadii ComputeRadii(const FArcMobileVisDistanceConfigFragment& DistConfig, float CellSize)
	{
		FMobileVisRadii R;
		R.ActorRadiusCells = FMath::CeilToInt(DistConfig.ActorRadius / CellSize);
		R.ISMRadiusCells = FMath::CeilToInt(DistConfig.ISMRadius / CellSize);
		const float HystMult = 1.f + DistConfig.HysteresisPercent / 100.f;
		R.ActorRadiusCellsLeave = FMath::CeilToInt(DistConfig.ActorRadius * HystMult / CellSize);
		R.ISMRadiusCellsLeave = FMath::CeilToInt(DistConfig.ISMRadius * HystMult / CellSize);
		return R;
	}
}

// ---------------------------------------------------------------------------
// 1. UArcMobileVisSourceTrackingProcessor
// ---------------------------------------------------------------------------

UArcMobileVisSourceTrackingProcessor::UArcMobileVisSourceTrackingProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcMobileVisSourceTrackingProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddTagRequirement<FArcMobileVisSourceTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcMobileVisSourceTrackingProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSubsystem)
	{
		return;
	}

	const float CellSize = Subsystem->GetCellSize();
	constexpr int32 SearchRadiusCells = 30;

	TArray<FMassEntityHandle> EntitiesToSignal;

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem, SignalSubsystem, CellSize, &EntitiesToSignal](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FVector Position = Transforms[EntityIt].GetTransform().GetLocation();
				const FIntVector NewCell = Subsystem->WorldToCell(Position);
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				const TMap<FMassEntityHandle, FIntVector>& SourcePositions = Subsystem->GetSourcePositions();
				const FIntVector* ExistingCell = SourcePositions.Find(Entity);

				if (!ExistingCell)
				{
					// Source not registered yet
					Subsystem->RegisterSource(Entity, NewCell);

					// Signal all entities in range of the new source
					TArray<FIntVector> NearbyCells;
					Subsystem->GetEntityCellsInRadius(NewCell, SearchRadiusCells, NearbyCells);
					for (const FIntVector& Cell : NearbyCells)
					{
						if (const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell))
						{
							EntitiesToSignal.Append(*Entities);
						}
					}
				}
				else if (*ExistingCell != NewCell)
				{
					const FIntVector OldCell = *ExistingCell;
					Subsystem->MoveSourceCell(Entity, OldCell, NewCell);

					// Signal entities around both old and new positions
					TArray<FIntVector> OldNearbyCells;
					Subsystem->GetEntityCellsInRadius(OldCell, SearchRadiusCells, OldNearbyCells);
					for (const FIntVector& Cell : OldNearbyCells)
					{
						if (const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell))
						{
							EntitiesToSignal.Append(*Entities);
						}
					}

					TArray<FIntVector> NewNearbyCells;
					Subsystem->GetEntityCellsInRadius(NewCell, SearchRadiusCells, NewNearbyCells);
					for (const FIntVector& Cell : NewNearbyCells)
					{
						if (const TArray<FMassEntityHandle>* Entities = Subsystem->GetEntitiesInCell(Cell))
						{
							EntitiesToSignal.Append(*Entities);
						}
					}
				}
			}
		});

	if (EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::MobileVisEntityCellChanged, EntitiesToSignal);
	}
}

// ---------------------------------------------------------------------------
// 2. UArcMobileVisEntityCellUpdateProcessor
// ---------------------------------------------------------------------------

UArcMobileVisEntityCellUpdateProcessor::UArcMobileVisEntityCellUpdateProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	ExecutionOrder.ExecuteAfter.Add(UArcMobileVisSourceTrackingProcessor::StaticClass()->GetFName());
}

void UArcMobileVisEntityCellUpdateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisDistanceConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisEntityCellUpdateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSubsystem)
	{
		return;
	}

	const float CellSize = Subsystem->GetCellSize();
	const float HalfCellSizeSq = (CellSize * 0.5f) * (CellSize * 0.5f);

	TArray<FMassEntityHandle> ChangedEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem, CellSize, HalfCellSizeSq, &ChangedEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisDistanceConfigFragment& DistConfig = Ctx.GetConstSharedFragment<FArcMobileVisDistanceConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				const FVector Position = Transforms[EntityIt].GetTransform().GetLocation();

				bool bShouldUpdate = false;
				if (DistConfig.CellUpdatePolicy == EArcMobileVisCellUpdatePolicy::EveryTick)
				{
					bShouldUpdate = true;
				}
				else // DistanceThreshold
				{
					const float DistSq = FVector::DistSquared(Position, Rep.LastPosition);
					bShouldUpdate = (DistSq > HalfCellSizeSq);
				}

				if (!bShouldUpdate)
				{
					continue;
				}

				const FIntVector NewCell = Subsystem->WorldToCell(Position);
				if (NewCell != Rep.GridCoords)
				{
					const FIntVector OldCell = Rep.GridCoords;
					const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

					Subsystem->MoveEntityCell(Entity, OldCell, NewCell);
					Rep.GridCoords = NewCell;
					Rep.LastPosition = Position;

					ChangedEntities.Add(Entity);
				}
				else
				{
					Rep.LastPosition = Position;
				}
			}
		});

	if (ChangedEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::MobileVisEntityCellChanged, ChangedEntities);
	}
}

// ---------------------------------------------------------------------------
// 3. UArcMobileVisEntityCellChangedProcessor (Signal: MobileVisEntityCellChanged)
// ---------------------------------------------------------------------------

UArcMobileVisEntityCellChangedProcessor::UArcMobileVisEntityCellChangedProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcMobileVisEntityCellChangedProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::MobileVisEntityCellChanged);
}

void UArcMobileVisEntityCellChangedProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcMobileVisLODFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisDistanceConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisEntityCellChangedProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSubsystem)
	{
		return;
	}

	const float CellSize = Subsystem->GetCellSize();

	TArray<FMassEntityHandle> UpgradeToActorEntities;
	TArray<FMassEntityHandle> DowngradeToISMEntities;
	TArray<FMassEntityHandle> UpgradeToISMEntities;
	TArray<FMassEntityHandle> DowngradeToNoneEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem, CellSize, &UpgradeToActorEntities, &DowngradeToISMEntities, &UpgradeToISMEntities, &DowngradeToNoneEntities](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetFragmentView<FArcMobileVisRepFragment>();
			TArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetMutableFragmentView<FArcMobileVisLODFragment>();
			const FArcMobileVisDistanceConfigFragment& DistConfig = Ctx.GetConstSharedFragment<FArcMobileVisDistanceConfigFragment>();

			const FMobileVisRadii Radii = ComputeRadii(DistConfig, CellSize);

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				FArcMobileVisLODFragment& LOD = LODFragments[EntityIt];

				const EArcMobileVisLOD DesiredLOD = Subsystem->EvaluateEntityLOD(
					Rep.GridCoords,
					Radii.ActorRadiusCells,
					Radii.ISMRadiusCells,
					Radii.ActorRadiusCellsLeave,
					Radii.ISMRadiusCellsLeave,
					LOD.CurrentLOD);

				if (DesiredLOD == LOD.CurrentLOD)
				{
					continue;
				}

				const EArcMobileVisLOD PrevLOD = LOD.CurrentLOD;
				LOD.PrevLOD = PrevLOD;
				LOD.CurrentLOD = DesiredLOD;

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Determine which transition signal to send
				if (DesiredLOD == EArcMobileVisLOD::Actor)
				{
					UpgradeToActorEntities.Add(Entity);
				}
				else if (DesiredLOD == EArcMobileVisLOD::StaticMesh && PrevLOD == EArcMobileVisLOD::Actor)
				{
					DowngradeToISMEntities.Add(Entity);
				}
				else if (DesiredLOD == EArcMobileVisLOD::StaticMesh && PrevLOD == EArcMobileVisLOD::None)
				{
					UpgradeToISMEntities.Add(Entity);
				}
				else if (DesiredLOD == EArcMobileVisLOD::None)
				{
					DowngradeToNoneEntities.Add(Entity);
				}
			}
		});

	if (UpgradeToActorEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::MobileVisUpgradeToActor, UpgradeToActorEntities);
	}
	if (DowngradeToISMEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::MobileVisDowngradeToISM, DowngradeToISMEntities);
	}
	if (UpgradeToISMEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::MobileVisUpgradeToISM, UpgradeToISMEntities);
	}
	if (DowngradeToNoneEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::MobileVisDowngradeToNone, DowngradeToNoneEntities);
	}
}

// ---------------------------------------------------------------------------
// 4. UArcMobileVisUpgradeToActorProcessor (Signal: MobileVisUpgradeToActor)
// ---------------------------------------------------------------------------

UArcMobileVisUpgradeToActorProcessor::UArcMobileVisUpgradeToActorProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcMobileVisUpgradeToActorProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::MobileVisUpgradeToActor);
}

void UArcMobileVisUpgradeToActorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisUpgradeToActorProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& ActorFragment = ActorFragments[EntityIt];

				if (Rep.bIsActorRepresentation)
				{
					continue;
				}

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Remove ISM instance if has one
				UStaticMesh* MeshToRemove = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : Config.StaticMesh.Get();
				if (MeshToRemove && Rep.ISMInstanceId != INDEX_NONE)
				{
					Subsystem->RemoveISMInstance(Rep.RegionCoord, Config.ISMManagerClass, MeshToRemove, Rep.ISMInstanceId, EntityManager);
					Rep.ISMInstanceId = INDEX_NONE;
				}
				Rep.CurrentISMMesh = nullptr;

				// Spawn actor
				if (Config.ActorClass)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					AActor* NewActor = World->SpawnActor<AActor>(Config.ActorClass, EntityTransform, SpawnParams);
					ActorFragment.SetAndUpdateHandleMap(Entity, NewActor, true);
					NewActor->FinishSpawning(EntityTransform);
				}

				Rep.bIsActorRepresentation = true;
			}
		});
}

// ---------------------------------------------------------------------------
// 5. UArcMobileVisDowngradeToISMProcessor (Signal: MobileVisDowngradeToISM)
// ---------------------------------------------------------------------------

UArcMobileVisDowngradeToISMProcessor::UArcMobileVisDowngradeToISMProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcMobileVisDowngradeToISMProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::MobileVisDowngradeToISM);
}

void UArcMobileVisDowngradeToISMProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisDowngradeToISMProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem, World](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& ActorFragment = ActorFragments[EntityIt];

				if (!Rep.bIsActorRepresentation)
				{
					continue;
				}

				// Destroy actor
				if (AActor* Actor = ActorFragment.GetMutable())
				{
					Actor->Destroy();
				}
				ActorFragment.ResetAndUpdateHandleMap();

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Compute region coord from current grid coords
				const FIntVector RegionCoord = Subsystem->CellToRegion(Rep.GridCoords);

				// Add ISM instance
				UStaticMesh* MeshToUse = Config.StaticMesh;
				if (MeshToUse)
				{
					Rep.ISMInstanceId = Subsystem->AddISMInstance(
						RegionCoord,
						Config.ISMManagerClass,
						MeshToUse,
						Config.MaterialOverrides,
						Config.bCastShadows,
						EntityTransform,
						Entity);
				}

				Rep.RegionCoord = RegionCoord;
				Rep.CurrentISMMesh = MeshToUse;
				Rep.bIsActorRepresentation = false;
			}
		});
}

// ---------------------------------------------------------------------------
// 6. UArcMobileVisUpgradeToISMProcessor (Signal: MobileVisUpgradeToISM)
// ---------------------------------------------------------------------------

UArcMobileVisUpgradeToISMProcessor::UArcMobileVisUpgradeToISMProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcMobileVisUpgradeToISMProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::MobileVisUpgradeToISM);
}

void UArcMobileVisUpgradeToISMProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisUpgradeToISMProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];

				if (Rep.ISMInstanceId != INDEX_NONE)
				{
					continue;
				}

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FIntVector RegionCoord = Subsystem->CellToRegion(Rep.GridCoords);

				UStaticMesh* MeshToUse = Config.StaticMesh;
				if (MeshToUse)
				{
					Rep.ISMInstanceId = Subsystem->AddISMInstance(
						RegionCoord,
						Config.ISMManagerClass,
						MeshToUse,
						Config.MaterialOverrides,
						Config.bCastShadows,
						EntityTransform,
						Entity);
				}

				Rep.RegionCoord = RegionCoord;
				Rep.CurrentISMMesh = MeshToUse;
				Rep.bIsActorRepresentation = false;
			}
		});
}

// ---------------------------------------------------------------------------
// 7. UArcMobileVisDowngradeToNoneProcessor (Signal: MobileVisDowngradeToNone)
// ---------------------------------------------------------------------------

UArcMobileVisDowngradeToNoneProcessor::UArcMobileVisDowngradeToNoneProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcMobileVisDowngradeToNoneProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::MobileVisDowngradeToNone);
}

void UArcMobileVisDowngradeToNoneProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisDowngradeToNoneProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];

				// Remove ISM if has one
				UStaticMesh* MeshToRemove = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : Config.StaticMesh.Get();
				if (MeshToRemove && Rep.ISMInstanceId != INDEX_NONE)
				{
					Subsystem->RemoveISMInstance(Rep.RegionCoord, Config.ISMManagerClass, MeshToRemove, Rep.ISMInstanceId, EntityManager);
				}

				Rep.ISMInstanceId = INDEX_NONE;
				Rep.CurrentISMMesh = nullptr;
				Rep.bIsActorRepresentation = false;
			}
		});
}

// ---------------------------------------------------------------------------
// 8. UArcMobileVisISMTransformUpdateProcessor
// ---------------------------------------------------------------------------

UArcMobileVisISMTransformUpdateProcessor::UArcMobileVisISMTransformUpdateProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcMobileVisISMTransformUpdateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMobileVisLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisISMTransformUpdateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const TConstArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetFragmentView<FArcMobileVisLODFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMobileVisLODFragment& LOD = LODFragments[EntityIt];
				if (LOD.CurrentLOD != EArcMobileVisLOD::StaticMesh)
				{
					continue;
				}

				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				if (Rep.ISMInstanceId == INDEX_NONE)
				{
					continue;
				}

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				UStaticMesh* MeshToUse = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : Config.StaticMesh.Get();

				// Check if entity crossed a region boundary
				const FIntVector NewRegionCoord = Subsystem->CellToRegion(Rep.GridCoords);
				if (NewRegionCoord != Rep.RegionCoord)
				{
					// Remove from old region
					if (MeshToUse)
					{
						Subsystem->RemoveISMInstance(Rep.RegionCoord, Config.ISMManagerClass, MeshToUse, Rep.ISMInstanceId, EntityManager);
					}

					// Add to new region
					const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
					if (MeshToUse)
					{
						Rep.ISMInstanceId = Subsystem->AddISMInstance(
							NewRegionCoord,
							Config.ISMManagerClass,
							MeshToUse,
							Config.MaterialOverrides,
							Config.bCastShadows,
							EntityTransform,
							Entity);
					}
					else
					{
						Rep.ISMInstanceId = INDEX_NONE;
					}

					Rep.RegionCoord = NewRegionCoord;
				}
				else
				{
					// Same region — just update the transform
					if (MeshToUse)
					{
						Subsystem->UpdateISMTransform(Rep.RegionCoord, Config.ISMManagerClass, MeshToUse, Rep.ISMInstanceId, EntityTransform);
					}
				}
			}
		});
}

// ---------------------------------------------------------------------------
// 9. UArcMobileVisEntityInitObserver (Observer: FArcMobileVisEntityTag Add)
// ---------------------------------------------------------------------------

UArcMobileVisEntityInitObserver::UArcMobileVisEntityInitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcMobileVisEntityTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcMobileVisEntityInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FArcMobileVisLODFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcMobileVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMobileVisDistanceConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisEntityInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	const float CellSize = Subsystem->GetCellSize();

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, CellSize, World](FMassExecutionContext& Ctx)
		{
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			TArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetMutableFragmentView<FArcMobileVisLODFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();
			const FArcMobileVisDistanceConfigFragment& DistConfig = Ctx.GetConstSharedFragment<FArcMobileVisDistanceConfigFragment>();

			const FMobileVisRadii Radii = ComputeRadii(DistConfig, CellSize);

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FMassActorFragment& ActorFragment = ActorFragments[EntityIt];
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				FArcMobileVisLODFragment& LOD = LODFragments[EntityIt];
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				const FVector Position = EntityTransform.GetLocation();
				const FIntVector GridCoords = Subsystem->WorldToCell(Position);

				Rep.GridCoords = GridCoords;
				Rep.LastPosition = Position;

				Subsystem->RegisterEntity(Entity, GridCoords);

				// Evaluate initial LOD
				const EArcMobileVisLOD InitialLOD = Subsystem->EvaluateEntityLOD(
					GridCoords,
					Radii.ActorRadiusCells,
					Radii.ISMRadiusCells,
					Radii.ActorRadiusCellsLeave,
					Radii.ISMRadiusCellsLeave,
					EArcMobileVisLOD::None);

				LOD.CurrentLOD = InitialLOD;
				LOD.PrevLOD = EArcMobileVisLOD::None;

				if (InitialLOD == EArcMobileVisLOD::Actor)
				{
					// Spawn actor
					if (Config.ActorClass)
					{
						FActorSpawnParameters SpawnParams;
						SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
						SpawnParams.bDeferConstruction = true;

						AActor* NewActor = World->SpawnActor<AActor>(Config.ActorClass, EntityTransform, SpawnParams);
						ActorFragment.SetAndUpdateHandleMap(Entity, NewActor, true);
						NewActor->FinishSpawning(EntityTransform);
					}

					Rep.bIsActorRepresentation = true;
					Rep.CurrentISMMesh = nullptr;
				}
				else if (InitialLOD == EArcMobileVisLOD::StaticMesh)
				{
					const FIntVector RegionCoord = Subsystem->CellToRegion(GridCoords);

					UStaticMesh* MeshToUse = Config.StaticMesh;
					if (MeshToUse)
					{
						Rep.ISMInstanceId = Subsystem->AddISMInstance(
							RegionCoord,
							Config.ISMManagerClass,
							MeshToUse,
							Config.MaterialOverrides,
							Config.bCastShadows,
							EntityTransform,
							Entity);
					}

					Rep.RegionCoord = RegionCoord;
					Rep.CurrentISMMesh = MeshToUse;
					Rep.bIsActorRepresentation = false;
				}
				else
				{
					// None — no representation
					Rep.bIsActorRepresentation = false;
					Rep.CurrentISMMesh = nullptr;
				}
			}
		});
}

// ---------------------------------------------------------------------------
// 10. UArcMobileVisEntityDeinitObserver (Observer: FArcMobileVisEntityTag Remove)
// ---------------------------------------------------------------------------

UArcMobileVisEntityDeinitObserver::UArcMobileVisEntityDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcMobileVisEntityTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcMobileVisEntityDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FArcMobileVisLODFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcMobileVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisEntityDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const TConstArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetFragmentView<FArcMobileVisLODFragment>();
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& ActorFragment = ActorFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				if (Rep.bIsActorRepresentation)
				{
					// Destroy actor
					if (AActor* Actor = ActorFragment.GetMutable())
					{
						Actor->Destroy();
					}
					ActorFragment.ResetAndUpdateHandleMap();
				}
				else
				{
					// Remove ISM instance
					UStaticMesh* MeshToRemove = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : Config.StaticMesh.Get();
					if (MeshToRemove && Rep.ISMInstanceId != INDEX_NONE)
					{
						Subsystem->RemoveISMInstance(Rep.RegionCoord, Config.ISMManagerClass, MeshToRemove, Rep.ISMInstanceId, EntityManager);
						Rep.ISMInstanceId = INDEX_NONE;
					}
				}

				Subsystem->UnregisterEntity(Entity, Rep.GridCoords);
			}
		});
}

// ---------------------------------------------------------------------------
// 11. UArcMobileVisSourceDeinitObserver (Observer: FArcMobileVisSourceTag Remove)
// ---------------------------------------------------------------------------

UArcMobileVisSourceDeinitObserver::UArcMobileVisSourceDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcMobileVisSourceTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcMobileVisSourceDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddTagRequirement<FArcMobileVisSourceTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcMobileVisSourceDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcMobileVisSubsystem* Subsystem = World->GetSubsystem<UArcMobileVisSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[Subsystem](FMassExecutionContext& Ctx)
		{
			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const TMap<FMassEntityHandle, FIntVector>& SourcePositions = Subsystem->GetSourcePositions();

				if (const FIntVector* Cell = SourcePositions.Find(Entity))
				{
					Subsystem->UnregisterSource(Entity, *Cell);
				}
			}
		});
}
