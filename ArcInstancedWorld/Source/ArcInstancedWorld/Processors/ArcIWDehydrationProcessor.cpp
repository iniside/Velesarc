// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Processors/ArcIWDehydrationProcessor.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWDehydration);

	const bool bSkipHydration = UArcIWSettings::Get()->bDisableActorHydration;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, Subsystem, Pool, bSkipHydration](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			TArray<FMassEntityHandle> PhysicsEntities;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];

				if (!Instance.bIsActorRepresentation)
				{
					continue;
				}

				if (bSkipHydration)
				{
					continue;
				}

				if (Subsystem->IsWithinDehydrationRadius(Instance.ActorGridCoords))
				{
					continue;
				}

				if (Instance.bKeepHydrated)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

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
				if (Subsystem->IsMeshCell(Instance.MeshGridCoords))
				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						MassISMPartition->ActivateMesh(Entity, Instance, Config, EntityTransform, EntityManager, Ctx);

						if (Subsystem->IsPhysicsCell(Instance.PhysicsGridCoords))
						{
							PhysicsEntities.Add(Entity);
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
