// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWDehydrationProcessor.h"

#include "ArcIWTypes.h"
#include "ArcIWPartitionActor.h"
#include "ArcIWMassISMPartitionActor.h"
#include "ArcIWSettings.h"
#include "ArcIWVisualizationSubsystem.h"
#include "ArcIWActorPoolSubsystem.h"
#include "ArcMass/ArcMassPhysicsLink.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWDehydrationProcessor)

// ---------------------------------------------------------------------------
// UArcIWDehydrationProcessor
// ---------------------------------------------------------------------------

UArcIWDehydrationProcessor::UArcIWDehydrationProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWDehydrationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWDehydrationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();

	const bool bMassISMNoHydration = UArcIWSettings::Get()->bUseMassISM && UArcIWSettings::Get()->bDisableActorHydration;

	TArray<FMassEntityHandle> PhysicsLinkEntities;
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, Pool, bMassISMNoHydration, &PhysicsLinkEntities](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];

				if (!Instance.bIsActorRepresentation)
				{
					continue;
				}

				if (bMassISMNoHydration)
				{
					continue;
				}

				if (Subsystem->IsWithinDehydrationRadius(Instance.GridCoords))
				{
					continue;
				}

				if (Instance.bKeepHydrated)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				{
					FArcMassPhysicsLinkFragment* LinkFragment =
						EntityManager.GetFragmentDataPtr<FArcMassPhysicsLinkFragment>(Entity);
					if (LinkFragment)
					{
						AArcIWPartitionActor::DetachPhysicsLinkEntries(*LinkFragment);
					}
				}

				// Release actor back to pool
				AActor* Actor = Instance.HydratedActor.Get();
				if (Actor)
				{
					if (Pool)
					{
						Pool->ReleaseActor(Actor);
					}
					else
					{
						Actor->Destroy();
					}
				}
				Instance.HydratedActor = nullptr;
				Instance.bIsActorRepresentation = false;

				// Re-add ISM instances if within ISM radius
				if (Subsystem->IsCellActive(Instance.GridCoords))
				{
					AArcIWPartitionActor* Partition = Cast<AArcIWPartitionActor>(Instance.PartitionActor.Get());
					if (Partition)
					{
						Partition->AddCompositeISMInstances(
							Instance.MeshSlotBase,
							EntityTransform,
							Config.MeshDescriptors,
							Instance.ISMInstanceIds,
							Instance.InstanceIndex);
						FArcMassPhysicsLinkFragment* LinkFragment =
							EntityManager.GetFragmentDataPtr<FArcMassPhysicsLinkFragment>(Entity);
						if (LinkFragment)
						{
							Partition->PopulatePhysicsLinkEntries(*LinkFragment, Instance.MeshSlotBase, Instance.ISMInstanceIds);
							PhysicsLinkEntities.Add(Entity);
						}
					}

					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						MassISMPartition->ActivateEntity(Entity, Instance, Config, EntityTransform, EntityManager);
						PhysicsLinkEntities.Add(Entity);
					}
				}
			}
		});

	if (PhysicsLinkEntities.Num() > 0)
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsLinkRequested, PhysicsLinkEntities);
		}
	}
}
