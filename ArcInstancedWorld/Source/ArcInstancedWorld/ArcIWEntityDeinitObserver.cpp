// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWEntityDeinitObserver.h"

#include "ArcIWTypes.h"
#include "ArcIWMassISMPartitionActor.h"
#include "ArcIWVisualizationSubsystem.h"
#include "ArcIWActorPoolSubsystem.h"
#include "ArcMass/ArcMassPhysicsLink.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"

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

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

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
					FArcMassPhysicsLinkFragment* LinkFragment =
						EntityManager.GetFragmentDataPtr<FArcMassPhysicsLinkFragment>(Entity);
					if (LinkFragment)
					{
						UE::ArcIW::DetachPhysicsLinkEntries(*LinkFragment);
					}
				}

				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						MassISMPartition->DeactivateMesh(Entity, Instance, Config, EntityManager);
						MassISMPartition->DeactivatePhysics(Entity, EntityManager);
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
				Subsystem->UnregisterEntity(Entity, Instance.GridCoords);
			}
		});
}
