// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWEntityInitObserver.h"

#include "ArcIWTypes.h"
#include "ArcIWMassISMPartitionActor.h"
#include "ArcIWSettings.h"
#include "ArcIWVisualizationSubsystem.h"
#include "ArcIWActorPoolSubsystem.h"
#include "ArcMass/ArcMassPhysicsLink.h"
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

	TArray<FMassEntityHandle> PhysicsLinkEntities;
	ObserverQuery.ForEachEntityChunk(Context,
		[Subsystem, &EntityManager, &PhysicsLinkEntities](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Compute and store grid coords
				const FVector Position = EntityTransform.GetLocation();
				Instance.GridCoords = Subsystem->WorldToCell(Position);

				// Register in the grid
				Subsystem->RegisterEntity(Entity, Position);

				const bool bIsMassISM = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get()) != nullptr;
				const bool bSkipHydration = bIsMassISM && UArcIWSettings::Get()->bDisableActorHydration;

				// If cell is within actor radius, acquire actor from pool
				if (!bSkipHydration && Subsystem->IsActorCell(Instance.GridCoords) && Config.ActorClass)
				{
					UArcIWActorPoolSubsystem* Pool = Ctx.GetWorld()->GetSubsystem<UArcIWActorPoolSubsystem>();
					if (Pool)
					{
						AActor* NewActor = Pool->AcquireActor(Config.ActorClass, EntityTransform, Entity);
						Instance.HydratedActor = NewActor;
					}
					Instance.bIsActorRepresentation = true;
					if (Instance.HydratedActor.IsValid())
					{
						FArcMassPhysicsLinkFragment* LinkFragment =
							EntityManager.GetFragmentDataPtr<FArcMassPhysicsLinkFragment>(Entity);
						if (LinkFragment)
						{
							ArcMassPhysicsLink::PopulateEntriesFromActor(*LinkFragment, Instance.HydratedActor.Get());
							PhysicsLinkEntities.Add(Entity);
						}
					}
				}
				// Else if cell is within ISM radius, create ISM instances
				else if (Subsystem->IsCellActive(Instance.GridCoords))
				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						if (Subsystem->IsMeshCell(Instance.GridCoords))
						{
							MassISMPartition->ActivateMesh(Entity, Instance, Config, EntityTransform, EntityManager, Ctx);

							if (Subsystem->IsPhysicsCell(Instance.GridCoords))
							{
								MassISMPartition->ActivatePhysics(Entity, Instance, Config, EntityTransform, EntityManager);
								PhysicsLinkEntities.Add(Entity);
							}
						}
					}
				}
			}
		});

	if (PhysicsLinkEntities.Num() > 0)
	{
		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsLinkRequested, PhysicsLinkEntities);
		}
	}
}
