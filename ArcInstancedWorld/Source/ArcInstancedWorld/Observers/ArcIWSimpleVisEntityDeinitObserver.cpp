// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Observers/ArcIWSimpleVisEntityDeinitObserver.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcInstancedWorld/Visualization/ArcIWSkinnedISMHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWSimpleVisEntityDeinitObserver)

UArcIWSimpleVisEntityDeinitObserver::UArcIWSimpleVisEntityDeinitObserver()
	: ObserverQuery{*this}
	, SkinnedObserverQuery{*this}
{
	ObservedTypes.Add(FArcIWSimpleVisEntityTag::StaticStruct());
	ObservedTypes.Add(FArcIWSimpleVisSkinnedTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcIWSimpleVisEntityDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcIWSimpleVisEntityTag>(EMassFragmentPresence::All);

	SkinnedObserverQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	SkinnedObserverQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	SkinnedObserverQuery.AddTagRequirement<FArcIWSimpleVisSkinnedTag>(EMassFragmentPresence::All);
}

void UArcIWSimpleVisEntityDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWSimpleVisEntityDeinit);

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();

			TArray<FMassEntityHandle> PhysicsEntities;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Collect for batch physics deactivation
				PhysicsEntities.Add(Entity);

				// Remove from ISM holder
				if (Instance.ISMInstanceIds.IsValidIndex(0) && Instance.ISMInstanceIds[0] != INDEX_NONE)
				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						MassISMPartition->DeactivateSimpleVisMesh(Entity, Instance, EntityManager);
					}
				}

				// Release hydrated actor back to pool
				AActor* HydratedActor = Instance.HydratedActor.Get();
				if (HydratedActor)
				{
					UArcIWActorPoolSubsystem* Pool = Ctx.GetWorld()->GetSubsystem<UArcIWActorPoolSubsystem>();
					if (Pool)
					{
						Pool->ReleaseActor(HydratedActor);
					}
					else
					{
						HydratedActor->Destroy();
					}
					Instance.HydratedActor = nullptr;
				}
				Instance.bIsActorRepresentation = false;

				// Unregister from subsystem grid
				Subsystem->UnregisterMeshEntity(Entity, Instance.MeshGridCoords);
				Subsystem->UnregisterPhysicsEntity(Entity, Instance.PhysicsGridCoords);
				Subsystem->UnregisterActorEntity(Entity, Instance.ActorGridCoords);
			}

			if (PhysicsEntities.Num() > 0)
			{
				UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
				if (SignalSubsystem)
				{
					SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyReleased, PhysicsEntities);
				}
			}
		});

	TSet<FMassEntityHandle> DirtySkinnedHolders;
	SkinnedObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, &DirtySkinnedHolders](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();

			TArray<FMassEntityHandle> PhysicsEntities;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				PhysicsEntities.Add(Entity);

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{

					if (Instance.ISMInstanceIds.IsValidIndex(0) && Instance.ISMInstanceIds[0] != INDEX_NONE)
					{
						FMassEntityHandle HolderEntity = MassISMPartition->DeactivateSimpleVisSkinnedMesh(Entity, Instance, EntityManager);
						if (HolderEntity.IsValid())
						{
							DirtySkinnedHolders.Add(HolderEntity);
						}
					}
				}

				AActor* HydratedActor = Instance.HydratedActor.Get();
				if (HydratedActor)
				{
					UArcIWActorPoolSubsystem* Pool = Ctx.GetWorld()->GetSubsystem<UArcIWActorPoolSubsystem>();
					if (Pool)
					{
						Pool->ReleaseActor(HydratedActor);
					}
					else
					{
						HydratedActor->Destroy();
					}
					Instance.HydratedActor = nullptr;
				}
				Instance.bIsActorRepresentation = false;

				Subsystem->UnregisterMeshEntity(Entity, Instance.MeshGridCoords);
				Subsystem->UnregisterPhysicsEntity(Entity, Instance.PhysicsGridCoords);
				Subsystem->UnregisterActorEntity(Entity, Instance.ActorGridCoords);
			}

			if (PhysicsEntities.Num() > 0)
			{
				UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
				if (SignalSubsystem)
				{
					SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyReleased, PhysicsEntities);
				}
			}
		});
	UE::ArcIW::SkinnedISM::FlushDirtyHolders(DirtySkinnedHolders, EntityManager);
}
