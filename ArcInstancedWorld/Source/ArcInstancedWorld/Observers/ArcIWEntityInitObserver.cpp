// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Observers/ArcIWEntityInitObserver.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWEntityInitObserver)

// ---------------------------------------------------------------------------
// UArcIWEntityInitObserver
// ---------------------------------------------------------------------------

UArcIWEntityInitObserver::UArcIWEntityInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcIWEntityTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcIWEntityInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWEntityInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWEntityInit);

	ObserverQuery.ForEachEntityChunk(Context,
		[Subsystem, &EntityManager](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			TArray<FMassEntityHandle> PhysicsEntities;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Compute and store grid coords
				const FVector Position = EntityTransform.GetLocation();
				Instance.MeshGridCoords = Subsystem->WorldToMeshCell(Position);
				Instance.PhysicsGridCoords = Subsystem->WorldToPhysicsCell(Position);
				Instance.ActorGridCoords = Subsystem->WorldToActorCell(Position);

				// Register in the grid
				Subsystem->RegisterMeshEntity(Entity, Position);
				Subsystem->RegisterPhysicsEntity(Entity, Position);
				Subsystem->RegisterActorEntity(Entity, Position);

				const bool bIsMassISM = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get()) != nullptr;
				const bool bSkipHydration = bIsMassISM && UArcIWSettings::Get()->bDisableActorHydration;

				// If cell is within actor radius, acquire actor from pool
				if (!bSkipHydration && Subsystem->IsActorCell(Instance.ActorGridCoords) && Config.ActorClass)
				{
					UArcIWActorPoolSubsystem* Pool = Ctx.GetWorld()->GetSubsystem<UArcIWActorPoolSubsystem>();
					if (Pool)
					{
						AActor* NewActor = Pool->AcquireActor(Config.ActorClass, EntityTransform, Entity);
						Instance.HydratedActor = NewActor;
					}
					Instance.bIsActorRepresentation = true;
				}
				// Else if cell is within ISM radius, create ISM instances
				else if (Subsystem->IsMeshCell(Instance.MeshGridCoords))
				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						if (Subsystem->IsMeshCell(Instance.MeshGridCoords))
						{
							MassISMPartition->ActivateMesh(Entity, Instance, Config, EntityTransform, EntityManager, Ctx);

							if (Subsystem->IsPhysicsCell(Instance.PhysicsGridCoords))
							{
								PhysicsEntities.Add(Entity);
							}
						}
					}
				}
			}

			if (PhysicsEntities.Num() > 0)
			{
				UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
				if (SignalSubsystem)
				{
					SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, PhysicsEntities);
				}
			}
		});

}
