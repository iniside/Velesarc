// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Processors/ArcIWActorActivateProcessor.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWActorActivateProcessor)

// ---------------------------------------------------------------------------
// UArcIWActorActivateProcessor — ISM -> Actor
// ---------------------------------------------------------------------------

UArcIWActorActivateProcessor::UArcIWActorActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWActorActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::ActorCellActivated);
}

void UArcIWActorActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWActorActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWActorActivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			TArray<FMassEntityHandle> PhysicsEntities;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				const bool bIsMassISM = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get()) != nullptr;
				if (bIsMassISM && UArcIWSettings::Get()->bDisableActorHydration)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{
					MassISMPartition->DeactivateMesh(Entity, Instance, Config, EntityManager);
					PhysicsEntities.Add(Entity);
				}

				// Acquire actor from pool
				if (Config.ActorClass)
				{
					UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
					if (Pool)
					{
						AActor* NewActor = Pool->AcquireActor(Config.ActorClass, EntityTransform, Entity);
						Instance.HydratedActor = NewActor;
					}
				}
				Instance.bIsActorRepresentation = true;
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
