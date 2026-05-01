// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisualizationProcessors.h"

#include "ArcMobileVisualization.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "MassSpawnerSubsystem.h"
#include "Mesh/MassEngineMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsBodyConfig.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "ArcMass/Physics/ArcMassPhysicsEntityLink.h"
#include "Components/PrimitiveComponent.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace ArcMobileVisPrivate
{
	void RunObjectFragmentInitializers(FMassEntityManager& EntityManager, FMassEntityHandle Entity, AActor& Actor, const FMassEntityTemplateID& TemplateID)
	{
		UWorld* World = EntityManager.GetWorld();
		if (!World)
		{
			return;
		}

		UMassSpawnerSubsystem* SpawnerSubsystem = World->GetSubsystem<UMassSpawnerSubsystem>();
		if (!SpawnerSubsystem)
		{
			return;
		}

		const FMassEntityTemplate* Template = SpawnerSubsystem->GetMassEntityTemplate(TemplateID);
		if (!Template)
		{
			return;
		}

		TConstArrayView<FMassEntityTemplateData::FObjectFragmentInitializerFunction> Initializers = Template->GetObjectFragmentInitializers();
		if (Initializers.IsEmpty())
		{
			return;
		}

		FMassEntityView EntityView(EntityManager, Entity);
		for (const FMassEntityTemplateData::FObjectFragmentInitializerFunction& Initializer : Initializers)
		{
			Initializer(Actor, EntityView, EMassTranslationDirection::MassToActor);
		}
	}

	FChaosUserEntityAppend* AttachEntityLinkToActor(AActor& Actor, FMassEntityHandle Entity)
	{
		UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
		if (!RootPrimitive)
		{
			return nullptr;
		}

		FBodyInstance* ActorBody = RootPrimitive->GetBodyInstance();
		if (!ActorBody)
		{
			return nullptr;
		}

		return ArcMassPhysicsEntityLink::Attach(*ActorBody, Entity);
	}

	void DetachEntityLinkFromActor(AActor& Actor, FChaosUserEntityAppend* Append)
	{
		if (!Append)
		{
			return;
		}

		UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor.GetRootComponent());
		if (!RootPrimitive)
		{
			return;
		}

		FBodyInstance* ActorBody = RootPrimitive->GetBodyInstance();
		if (!ActorBody)
		{
			return;
		}

		ArcMassPhysicsEntityLink::Detach(*ActorBody, Append);
	}
}

namespace
{
	struct FMobileVisRadii
	{
		int32 ActorRadiusCells;
		int32 ISMRadiusCells;
		int32 ActorRadiusCellsLeave;
		int32 ISMRadiusCellsLeave;
		int32 PhysicsRadiusCells;
		int32 PhysicsRadiusCellsLeave;
	};

	FMobileVisRadii ComputeRadii(const FArcMobileVisDistanceConfigFragment& DistConfig, float CellSize)
	{
		FMobileVisRadii R;
		R.ActorRadiusCells = FMath::CeilToInt(DistConfig.ActorRadius / CellSize);
		R.ISMRadiusCells = FMath::CeilToInt(DistConfig.ISMRadius / CellSize);
		const float HystMult = 1.f + DistConfig.HysteresisPercent / 100.f;
		R.ActorRadiusCellsLeave = FMath::CeilToInt(DistConfig.ActorRadius * HystMult / CellSize);
		R.ISMRadiusCellsLeave = FMath::CeilToInt(DistConfig.ISMRadius * HystMult / CellSize);
		R.PhysicsRadiusCells = FMath::CeilToInt(DistConfig.PhysicsRadius / CellSize);
		R.PhysicsRadiusCellsLeave = FMath::CeilToInt(DistConfig.PhysicsRadius * HystMult / CellSize);
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisSourceTracking);

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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisEntityCellUpdate);

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
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisEntityCellChanged);

	const float CellSize = Subsystem->GetCellSize();

	TArray<FMassEntityHandle> UpgradeToActorEntities;
	TArray<FMassEntityHandle> DowngradeToISMEntities;
	TArray<FMassEntityHandle> UpgradeToISMEntities;
	TArray<FMassEntityHandle> DowngradeToNoneEntities;
	TArray<FMassEntityHandle> PhysicsRequestEntities;
	TArray<FMassEntityHandle> PhysicsReleaseEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem, CellSize, &UpgradeToActorEntities, &DowngradeToISMEntities, &UpgradeToISMEntities, &DowngradeToNoneEntities, &PhysicsRequestEntities, &PhysicsReleaseEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			TArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetMutableFragmentView<FArcMobileVisLODFragment>();
			const FArcMobileVisDistanceConfigFragment& DistConfig = Ctx.GetConstSharedFragment<FArcMobileVisDistanceConfigFragment>();

			const FMobileVisRadii Radii = ComputeRadii(DistConfig, CellSize);

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				FArcMobileVisLODFragment& LOD = LODFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				const EArcMobileVisLOD DesiredLOD = Subsystem->EvaluateEntityLOD(
					Rep.GridCoords,
					Radii.ActorRadiusCells,
					Radii.ISMRadiusCells,
					Radii.ActorRadiusCellsLeave,
					Radii.ISMRadiusCellsLeave,
					LOD.CurrentLOD);

				if (DesiredLOD != LOD.CurrentLOD)
				{
					const EArcMobileVisLOD PrevLOD = LOD.CurrentLOD;
					LOD.PrevLOD = PrevLOD;
					LOD.CurrentLOD = DesiredLOD;

					// Determine which transition signal to send
					if (DesiredLOD == EArcMobileVisLOD::Actor)
					{
						UpgradeToActorEntities.Add(Entity);
					}
					else if (DesiredLOD == EArcMobileVisLOD::InstancedMesh && PrevLOD == EArcMobileVisLOD::Actor)
					{
						DowngradeToISMEntities.Add(Entity);
					}
					else if (DesiredLOD == EArcMobileVisLOD::InstancedMesh && PrevLOD == EArcMobileVisLOD::None)
					{
						UpgradeToISMEntities.Add(Entity);
					}
					else if (DesiredLOD == EArcMobileVisLOD::None)
					{
						DowngradeToNoneEntities.Add(Entity);
					}
				}

				// Evaluate physics radius transition (independent of LOD)
				const bool bWantsPhysics = Subsystem->EvaluateEntityPhysics(
					Rep.GridCoords,
					Radii.PhysicsRadiusCells,
					Radii.PhysicsRadiusCellsLeave,
					Rep.bHasPhysicsBody);

				if (bWantsPhysics && !Rep.bHasPhysicsBody)
				{
					Rep.bHasPhysicsBody = true;
					PhysicsRequestEntities.Add(Entity);
				}
				else if (!bWantsPhysics && Rep.bHasPhysicsBody)
				{
					Rep.bHasPhysicsBody = false;
					PhysicsReleaseEntities.Add(Entity);
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
	if (PhysicsRequestEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, PhysicsRequestEntities);
	}
	if (PhysicsReleaseEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyReleased, PhysicsReleaseEntities);
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
	EntityQuery.AddConstSharedRequirement<FArcMobileVisTemplateIDFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
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

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisUpgradeToActor);

	TArray<FMassEntityHandle> PhysicsReleaseEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World, &PhysicsReleaseEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();
			const FArcMobileVisTemplateIDFragment& TemplateIDConfig = Ctx.GetConstSharedFragment<FArcMobileVisTemplateIDFragment>();
			const FArcMassStaticMeshConfigFragment* StaticMeshConfigFrag = Ctx.GetConstSharedFragmentPtr<FArcMassStaticMeshConfigFragment>();
			UStaticMesh* BaseMesh = StaticMeshConfigFrag ? const_cast<UStaticMesh*>(StaticMeshConfigFrag->Mesh.Get()) : nullptr;

			const bool bHasPhysicsFragment = Ctx.DoesArchetypeHaveFragment<FArcMassPhysicsBodyFragment>();

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

				// Remove ISM/ISKM instance if has one
				if (Config.bUseSkinnedMesh && Rep.ISKMInstanceId.IsValid())
				{
					Subsystem->RemoveISKMInstance(Rep.RegionCoord, Config.ISMManagerClass, Config.SkinnedAsset, Rep.ISKMInstanceId, EntityManager);
					Rep.ISKMInstanceId = FPrimitiveInstanceId();
				}
				else
				{
					UStaticMesh* MeshToRemove = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : BaseMesh;
					if (MeshToRemove && Rep.ISMInstanceId != INDEX_NONE)
					{
						Subsystem->RemoveISMInstance(Rep.RegionCoord, Config.ISMManagerClass, MeshToRemove, Rep.ISMInstanceId, EntityManager);
						Rep.ISMInstanceId = INDEX_NONE;
					}
				}
				Rep.CurrentISMMesh = nullptr;

				// Release standalone physics body before spawning actor (actor has its own collision)
				if (Rep.bHasPhysicsBody && bHasPhysicsFragment)
				{
					PhysicsReleaseEntities.Add(Entity);
				}

				// Spawn actor
				if (Config.ActorClass)
				{
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					AActor* NewActor = World->SpawnActor<AActor>(Config.ActorClass, EntityTransform, SpawnParams);
					ActorFragment.SetAndUpdateHandleMap(Entity, NewActor, true);
					NewActor->FinishSpawning(EntityTransform);

					ArcMobileVisPrivate::RunObjectFragmentInitializers(EntityManager, Entity, *NewActor, TemplateIDConfig.TemplateID);

					// Attach entity link to actor's root body for hit resolution
					Rep.ActorBodyEntityLink = ArcMobileVisPrivate::AttachEntityLinkToActor(*NewActor, Entity);
				}

				Rep.bIsActorRepresentation = true;
			}
		});

	if (SignalSubsystem && PhysicsReleaseEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyReleased, PhysicsReleaseEntities);
	}
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
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
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

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisDowngradeToISM);

	TArray<FMassEntityHandle> PhysicsRequestEntities;

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem, World, &PhysicsRequestEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();
			const FArcMassStaticMeshConfigFragment* StaticMeshConfigFrag = Ctx.GetConstSharedFragmentPtr<FArcMassStaticMeshConfigFragment>();
			UStaticMesh* BaseMesh = StaticMeshConfigFrag ? const_cast<UStaticMesh*>(StaticMeshConfigFrag->Mesh.Get()) : nullptr;

			const bool bHasPhysicsFragment = Ctx.DoesArchetypeHaveFragment<FArcMassPhysicsBodyFragment>();

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
					ArcMobileVisPrivate::DetachEntityLinkFromActor(*Actor, Rep.ActorBodyEntityLink);
					Rep.ActorBodyEntityLink = nullptr;

					Actor->Destroy();
				}
				ActorFragment.ResetAndUpdateHandleMap();

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Compute region coord from current grid coords
				const FIntVector RegionCoord = Subsystem->CellToRegion(Rep.GridCoords);

				// Add ISM/ISKM instance
				if (Config.bUseSkinnedMesh && Config.SkinnedAsset)
				{
					Rep.ISKMInstanceId = Subsystem->AddISKMInstance(
						RegionCoord,
						Config.ISMManagerClass,
						Config.SkinnedAsset,
						Config.TransformProvider,
						Config.MaterialOverrides,
						Config.bCastShadows,
						Transforms[EntityIt].GetTransform(),
						Rep.AnimationIndex,
						Entity);
					Rep.RegionCoord = RegionCoord;
					Rep.CurrentISMMesh = nullptr;
					Rep.bIsActorRepresentation = false;
				}
				else
				{
					UStaticMesh* MeshToUse = BaseMesh;
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

				// Re-request standalone physics body now that actor is gone
				if (Rep.bHasPhysicsBody && bHasPhysicsFragment)
				{
					PhysicsRequestEntities.Add(Entity);
				}
			}
		});

	if (SignalSubsystem && PhysicsRequestEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, PhysicsRequestEntities);
	}
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
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::Optional);
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisUpgradeToISM);

	EntityQuery.ForEachEntityChunk(Context,
		[Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();
			const FArcMassStaticMeshConfigFragment* StaticMeshConfigFrag = Ctx.GetConstSharedFragmentPtr<FArcMassStaticMeshConfigFragment>();
			UStaticMesh* BaseMesh = StaticMeshConfigFrag ? const_cast<UStaticMesh*>(StaticMeshConfigFrag->Mesh.Get()) : nullptr;

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

				if (Config.bUseSkinnedMesh && Config.SkinnedAsset)
				{
					Rep.ISKMInstanceId = Subsystem->AddISKMInstance(
						RegionCoord,
						Config.ISMManagerClass,
						Config.SkinnedAsset,
						Config.TransformProvider,
						Config.MaterialOverrides,
						Config.bCastShadows,
						Transforms[EntityIt].GetTransform(),
						Rep.AnimationIndex,
						Entity);
					Rep.RegionCoord = RegionCoord;
					Rep.CurrentISMMesh = nullptr;
					Rep.bIsActorRepresentation = false;
				}
				else
				{
					UStaticMesh* MeshToUse = BaseMesh;
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
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::Optional);
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisDowngradeToNone);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();
			const FArcMassStaticMeshConfigFragment* StaticMeshConfigFrag = Ctx.GetConstSharedFragmentPtr<FArcMassStaticMeshConfigFragment>();
			UStaticMesh* BaseMesh = StaticMeshConfigFrag ? const_cast<UStaticMesh*>(StaticMeshConfigFrag->Mesh.Get()) : nullptr;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];

				// Remove ISM/ISKM if has one
				if (Config.bUseSkinnedMesh && Rep.ISKMInstanceId.IsValid())
				{
					Subsystem->RemoveISKMInstance(Rep.RegionCoord, Config.ISMManagerClass, Config.SkinnedAsset, Rep.ISKMInstanceId, EntityManager);
					Rep.ISKMInstanceId = FPrimitiveInstanceId();
				}
				else
				{
					UStaticMesh* MeshToRemove = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : BaseMesh;
					if (MeshToRemove && Rep.ISMInstanceId != INDEX_NONE)
					{
						Subsystem->RemoveISMInstance(Rep.RegionCoord, Config.ISMManagerClass, MeshToRemove, Rep.ISMInstanceId, EntityManager);
					}

					Rep.ISMInstanceId = INDEX_NONE;
				}

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
	EntityQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::Optional);
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisISMTransformUpdate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const TConstArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetFragmentView<FArcMobileVisLODFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();
			const FArcMassStaticMeshConfigFragment* StaticMeshConfigFrag = Ctx.GetConstSharedFragmentPtr<FArcMassStaticMeshConfigFragment>();
			UStaticMesh* BaseMesh = StaticMeshConfigFrag ? const_cast<UStaticMesh*>(StaticMeshConfigFrag->Mesh.Get()) : nullptr;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMobileVisLODFragment& LOD = LODFragments[EntityIt];
				if (LOD.CurrentLOD != EArcMobileVisLOD::InstancedMesh)
				{
					continue;
				}

				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				if (Config.bUseSkinnedMesh && Rep.ISKMInstanceId.IsValid())
				{
					Subsystem->UpdateISKMTransform(
						Rep.RegionCoord,
						Config.ISMManagerClass,
						Config.SkinnedAsset,
						Rep.ISKMInstanceId,
						EntityTransform,
						Rep.AnimationIndex);
				}
				else if (Rep.ISMInstanceId != INDEX_NONE)
				{
					UStaticMesh* MeshToUse = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : BaseMesh;

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
			}
		});
}

// ---------------------------------------------------------------------------
// 9. UArcMobileVisEntityInitObserver (Observer: FArcMobileVisEntityTag Add)
// ---------------------------------------------------------------------------

UArcMobileVisEntityInitObserver::UArcMobileVisEntityInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMobileVisEntityTag::StaticStruct());
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
	ObserverQuery.AddConstSharedRequirement<FArcMobileVisTemplateIDFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::Optional);
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

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
	if (!SignalSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisEntityInit);

	const float CellSize = Subsystem->GetCellSize();

	TArray<FMassEntityHandle> PhysicsRequestEntities;

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, CellSize, World, &PhysicsRequestEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			TArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetMutableFragmentView<FArcMobileVisLODFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();
			const FArcMobileVisTemplateIDFragment& TemplateIDConfig = Ctx.GetConstSharedFragment<FArcMobileVisTemplateIDFragment>();
			const FArcMassStaticMeshConfigFragment* StaticMeshConfigFrag = Ctx.GetConstSharedFragmentPtr<FArcMassStaticMeshConfigFragment>();
			UStaticMesh* BaseMesh = StaticMeshConfigFrag ? const_cast<UStaticMesh*>(StaticMeshConfigFrag->Mesh.Get()) : nullptr;
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

						ArcMobileVisPrivate::RunObjectFragmentInitializers(EntityManager, Entity, *NewActor, TemplateIDConfig.TemplateID);

						// Attach entity link to actor body — do NOT request standalone physics body
						Rep.ActorBodyEntityLink = ArcMobileVisPrivate::AttachEntityLinkToActor(*NewActor, Entity);
					}

					Rep.bIsActorRepresentation = true;
					Rep.CurrentISMMesh = nullptr;
				}
				else if (InitialLOD == EArcMobileVisLOD::InstancedMesh)
				{
					const FIntVector RegionCoord = Subsystem->CellToRegion(GridCoords);
					Rep.RegionCoord = RegionCoord;

					if (Config.bUseSkinnedMesh && Config.SkinnedAsset)
					{
						Rep.ISKMInstanceId = Subsystem->AddISKMInstance(
							RegionCoord,
							Config.ISMManagerClass,
							Config.SkinnedAsset,
							Config.TransformProvider,
							Config.MaterialOverrides,
							Config.bCastShadows,
							EntityTransform,
							Rep.AnimationIndex,
							Entity);
						Rep.CurrentISMMesh = nullptr;
					}
					else
					{
						UStaticMesh* MeshToUse = BaseMesh;
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
						Rep.CurrentISMMesh = MeshToUse;
					}

					Rep.bIsActorRepresentation = false;
				}
				else
				{
					// None — no representation
					Rep.bIsActorRepresentation = false;
					Rep.CurrentISMMesh = nullptr;
				}

				// Evaluate physics radius (independent of LOD)
				const bool bWantsPhysics = Subsystem->EvaluateEntityPhysics(
					GridCoords,
					Radii.PhysicsRadiusCells,
					Radii.PhysicsRadiusCellsLeave,
					false);

				if (bWantsPhysics)
				{
					Rep.bHasPhysicsBody = true;

					// Only request standalone body if NOT at Actor LOD
					// (actor has its own collision; entity link already attached above)
					if (InitialLOD != EArcMobileVisLOD::Actor)
					{
						PhysicsRequestEntities.Add(Entity);
					}
				}
			}
		});

	if (PhysicsRequestEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, PhysicsRequestEntities);
	}
}

// ---------------------------------------------------------------------------
// 10. UArcMobileVisEntityDeinitObserver (Observer: FArcMobileVisEntityTag Remove)
// ---------------------------------------------------------------------------

UArcMobileVisEntityDeinitObserver::UArcMobileVisEntityDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMobileVisEntityTag::StaticStruct());
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
	ObserverQuery.AddConstSharedRequirement<FArcMassStaticMeshConfigFragment>(EMassFragmentPresence::Optional);
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

	UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisEntityDeinit);

	TArray<FMassEntityHandle> PhysicsReleaseEntities;

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, &PhysicsReleaseEntities](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			const TConstArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetFragmentView<FArcMobileVisLODFragment>();
			TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();
			const FArcMassStaticMeshConfigFragment* StaticMeshConfigFrag = Ctx.GetConstSharedFragmentPtr<FArcMassStaticMeshConfigFragment>();
			UStaticMesh* BaseMesh = StaticMeshConfigFrag ? const_cast<UStaticMesh*>(StaticMeshConfigFrag->Mesh.Get()) : nullptr;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& ActorFragment = ActorFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				if (Rep.bIsActorRepresentation)
				{
					if (AActor* Actor = ActorFragment.GetMutable())
					{
						ArcMobileVisPrivate::DetachEntityLinkFromActor(*Actor, Rep.ActorBodyEntityLink);
						Rep.ActorBodyEntityLink = nullptr;
						Actor->Destroy();
					}
					ActorFragment.ResetAndUpdateHandleMap();
				}
				else
				{
					if (Config.bUseSkinnedMesh && Rep.ISKMInstanceId.IsValid())
					{
						Subsystem->RemoveISKMInstance(Rep.RegionCoord, Config.ISMManagerClass, Config.SkinnedAsset, Rep.ISKMInstanceId, EntityManager);
						Rep.ISKMInstanceId = FPrimitiveInstanceId();
					}
					else
					{
						UStaticMesh* MeshToRemove = Rep.CurrentISMMesh ? Rep.CurrentISMMesh.Get() : BaseMesh;
						if (MeshToRemove && Rep.ISMInstanceId != INDEX_NONE)
						{
							Subsystem->RemoveISMInstance(Rep.RegionCoord, Config.ISMManagerClass, MeshToRemove, Rep.ISMInstanceId, EntityManager);
							Rep.ISMInstanceId = INDEX_NONE;
						}
					}
				}

				if (Rep.bHasPhysicsBody)
				{
					Rep.bHasPhysicsBody = false;
					PhysicsReleaseEntities.Add(Entity);
				}

				Subsystem->UnregisterEntity(Entity, Rep.GridCoords);
			}
		});

	if (SignalSubsystem && PhysicsReleaseEntities.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyReleased, PhysicsReleaseEntities);
	}
}

// ---------------------------------------------------------------------------
// 11. UArcMobileVisSourceDeinitObserver (Observer: FArcMobileVisSourceTag Remove)
// ---------------------------------------------------------------------------

UArcMobileVisSourceDeinitObserver::UArcMobileVisSourceDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMobileVisSourceTag::StaticStruct());
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisSourceDeinit);

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
