// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCompositeVisualizationProcessors.h"

#include "ArcCompositeVisualization.h"
#include "ArcMass/Visualization/ArcMassEntityVisualization.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

// ---------------------------------------------------------------------------
// UArcCompositeVisEntityInitObserver
// ---------------------------------------------------------------------------

UArcCompositeVisEntityInitObserver::UArcCompositeVisEntityInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcCompositeVisEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcCompositeVisEntityInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcCompositeVisRepFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcCompositeVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcCompositeVisEntityTag>(EMassFragmentPresence::All);
}

void UArcCompositeVisEntityInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCompositeVisEntityInit);

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, World](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcCompositeVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcCompositeVisRepFragment>();
			TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
			const FArcCompositeVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcCompositeVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcCompositeVisRepFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];
				const FTransform& EntityTransform = TransformFragments[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				const FVector Position = EntityTransform.GetLocation();
				Rep.GridCoords = Subsystem->GetMeshGrid().WorldToCell(Position);

				Subsystem->RegisterMeshEntity(Entity, Position);

				if (Subsystem->IsMeshActiveCellCoord(Rep.GridCoords))
				{
					if (!Config.ActorClass)
					{
						Rep.bIsActorRepresentation = false;
						continue;
					}

					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					AActor* NewActor = World->SpawnActor<AActor>(Config.ActorClass, EntityTransform, SpawnParams);
					if (NewActor)
					{
						MassActorFragment.SetAndUpdateHandleMap(Entity, NewActor, true);
						NewActor->FinishSpawning(EntityTransform);

						UE::ArcMass::CompositeVis::CaptureActorMeshData(NewActor, Rep.MeshEntries);
						Rep.bMeshDataCaptured = true;
						Rep.bIsActorRepresentation = true;
					}
				}
				else
				{
					// Cell is inactive — leave entity with no representation.
					// ISM instances will be created on first deactivation after mesh data is captured.
					Rep.bIsActorRepresentation = false;
				}
			}
		});
}

// ---------------------------------------------------------------------------
// UArcCompositeVisActivateProcessor — ISM → Actor
// ---------------------------------------------------------------------------

UArcCompositeVisActivateProcessor::UArcCompositeVisActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcCompositeVisActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshActivated);
}

void UArcCompositeVisActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcCompositeVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcCompositeVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcCompositeVisEntityTag>(EMassFragmentPresence::All);
}

void UArcCompositeVisActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* VisSubsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!VisSubsystem)
	{
		return;
	}

	UArcCompositeVisSubsystem* CompositeSubsystem = World->GetSubsystem<UArcCompositeVisSubsystem>();
	if (!CompositeSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCompositeVisActivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, VisSubsystem, CompositeSubsystem, World](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcCompositeVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcCompositeVisRepFragment>();
			TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
			const FArcCompositeVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcCompositeVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcCompositeVisRepFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];

				if (Rep.bIsActorRepresentation)
				{
					continue;
				}

				if (!Config.ActorClass)
				{
					continue;
				}

				const FTransform& EntityTransform = TransformFragments[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				FActorSpawnParameters SpawnParams;
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				SpawnParams.bDeferConstruction = true;

				AActor* NewActor = World->SpawnActor<AActor>(Config.ActorClass, EntityTransform, SpawnParams);
				if (!NewActor)
				{
					continue;
				}

				MassActorFragment.SetAndUpdateHandleMap(Entity, NewActor, true);
				NewActor->FinishSpawning(EntityTransform);

				// Remove all ISM instances that were created when the cell was inactive
				for (FArcCompositeVisInstanceEntry& Entry : Rep.MeshEntries)
				{
					if (Entry.ISMInstanceId != INDEX_NONE && Entry.Mesh)
					{
						CompositeSubsystem->RemoveISMInstance(
							Rep.GridCoords,
							Config.ISMManagerClass,
							Entry.Mesh,
							Entry.ISMInstanceId,
							EntityManager);
						Entry.ISMInstanceId = INDEX_NONE;
					}
				}

				if (!Rep.bMeshDataCaptured)
				{
					UE::ArcMass::CompositeVis::CaptureActorMeshData(NewActor, Rep.MeshEntries);
					Rep.bMeshDataCaptured = true;
				}

				Rep.bIsActorRepresentation = true;
			}
		});
}

// ---------------------------------------------------------------------------
// UArcCompositeVisDeactivateProcessor — Actor → ISM
// ---------------------------------------------------------------------------

UArcCompositeVisDeactivateProcessor::UArcCompositeVisDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcCompositeVisDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::VisMeshDeactivated);
}

void UArcCompositeVisDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcCompositeVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcCompositeVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcCompositeVisEntityTag>(EMassFragmentPresence::All);
}

void UArcCompositeVisDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* VisSubsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!VisSubsystem)
	{
		return;
	}

	UArcCompositeVisSubsystem* CompositeSubsystem = World->GetSubsystem<UArcCompositeVisSubsystem>();
	if (!CompositeSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCompositeVisDeactivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, VisSubsystem, CompositeSubsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcCompositeVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcCompositeVisRepFragment>();
			TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
			const FArcCompositeVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcCompositeVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcCompositeVisRepFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];

				if (!Rep.bIsActorRepresentation)
				{
					continue;
				}

				const FTransform& EntityTransform = TransformFragments[EntityIt].GetTransform();
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				if (Rep.bMeshDataCaptured)
				{
					// Add ISM instances for all captured mesh entries
					for (FArcCompositeVisInstanceEntry& Entry : Rep.MeshEntries)
					{
						CompositeSubsystem->AddISMInstance(
							Rep.GridCoords,
							Config.ISMManagerClass,
							Config.bCastShadows,
							EntityTransform,
							Entity,
							Entry.EntryIndex,
							Entry);
					}

					// Destroy actor and reset handle
					if (AActor* Actor = MassActorFragment.GetMutable())
					{
						Actor->Destroy();
					}
					MassActorFragment.ResetAndUpdateHandleMap();
				}
				else
				{
					// Mesh data not captured — entity goes invisible (no ISM)
					if (AActor* Actor = MassActorFragment.GetMutable())
					{
						Actor->Destroy();
					}
					MassActorFragment.ResetAndUpdateHandleMap();
				}

				Rep.bIsActorRepresentation = false;
			}
		});
}

// ---------------------------------------------------------------------------
// UArcCompositeVisEntityDeinitObserver
// ---------------------------------------------------------------------------

UArcCompositeVisEntityDeinitObserver::UArcCompositeVisEntityDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcCompositeVisEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcCompositeVisEntityDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcCompositeVisRepFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcCompositeVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcCompositeVisEntityTag>(EMassFragmentPresence::All);
}

void UArcCompositeVisEntityDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	UArcEntityVisualizationSubsystem* VisSubsystem = World->GetSubsystem<UArcEntityVisualizationSubsystem>();
	if (!VisSubsystem)
	{
		return;
	}

	UArcCompositeVisSubsystem* CompositeSubsystem = World->GetSubsystem<UArcCompositeVisSubsystem>();
	if (!CompositeSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCompositeVisEntityDeinit);

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, VisSubsystem, CompositeSubsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcCompositeVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcCompositeVisRepFragment>();
			TArrayView<FMassActorFragment> MassActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();
			const FArcCompositeVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcCompositeVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcCompositeVisRepFragment& Rep = RepFragments[EntityIt];
				FMassActorFragment& MassActorFragment = MassActorFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				if (Rep.bIsActorRepresentation)
				{
					if (AActor* Actor = MassActorFragment.GetMutable())
					{
						Actor->Destroy();
					}
					MassActorFragment.ResetAndUpdateHandleMap();
				}
				else
				{
					// ISM mode — remove all ISM instances
					for (FArcCompositeVisInstanceEntry& Entry : Rep.MeshEntries)
					{
						if (Entry.ISMInstanceId != INDEX_NONE && Entry.Mesh)
						{
							CompositeSubsystem->RemoveISMInstance(
								Rep.GridCoords,
								Config.ISMManagerClass,
								Entry.Mesh,
								Entry.ISMInstanceId,
								EntityManager);
							Entry.ISMInstanceId = INDEX_NONE;
						}
					}
				}

				VisSubsystem->UnregisterMeshEntity(Entity, Rep.GridCoords);
			}
		});
}
