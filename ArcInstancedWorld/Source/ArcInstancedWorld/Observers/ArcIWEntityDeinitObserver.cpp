// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Observers/ArcIWEntityDeinitObserver.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWEntityDeinitObserver)

// ---------------------------------------------------------------------------
// UArcIWEntityDeinitObserver
// ---------------------------------------------------------------------------

UArcIWEntityDeinitObserver::UArcIWEntityDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcIWEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcIWEntityDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWEntityDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWEntityDeinit);

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			TArray<FMassEntityHandle> PhysicsEntities;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Remove ISM instances if any are valid
				bool bHasValidInstances = false;
				for (int32 InstanceId : Instance.ISMInstanceIds)
				{
					if (InstanceId != INDEX_NONE)
					{
						bHasValidInstances = true;
						break;
					}
				}

				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						MassISMPartition->DeactivateMesh(Entity, Instance, Config, EntityManager);
						PhysicsEntities.Add(Entity);
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
}
