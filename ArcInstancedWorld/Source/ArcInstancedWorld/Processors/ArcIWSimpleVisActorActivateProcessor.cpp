// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Processors/ArcIWSimpleVisActorActivateProcessor.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcInstancedWorld/Visualization/ArcIWSkinnedISMHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWSimpleVisActorActivateProcessor)

UArcIWSimpleVisActorActivateProcessor::UArcIWSimpleVisActorActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWSimpleVisActorActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::ActorCellActivated);
}

void UArcIWSimpleVisActorActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWSimpleVisEntityTag>(EMassFragmentPresence::All);

	SkinnedEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	SkinnedEntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddTagRequirement<FArcIWSimpleVisSkinnedTag>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcMassSkinnedMeshFragment>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
}

void UArcIWSimpleVisActorActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWSimpleVisActorActivate);

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

				if (UArcIWSettings::Get()->bDisableActorHydration)
				{
					continue;
				}

				if (!Config.ActorClass)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				// Deactivate physics (actor will manage its own)
				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{
					PhysicsEntities.Add(Entity);

					// Remove from ISM holder
					MassISMPartition->DeactivateSimpleVisMesh(Entity, Instance, EntityManager);
				}

				// Acquire actor from pool
				UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
				if (Pool)
				{
					AActor* NewActor = Pool->AcquireActor(Config.ActorClass, EntityTransform, Entity);
					Instance.HydratedActor = NewActor;
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

	TSet<FMassEntityHandle> DirtySkinnedHolders;
	SkinnedEntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World, &DirtySkinnedHolders](FMassExecutionContext& Ctx)
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

				if (UArcIWSettings::Get()->bDisableActorHydration)
				{
					continue;
				}

				if (!Config.ActorClass)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{
					PhysicsEntities.Add(Entity);

					FMassEntityHandle HolderEntity = MassISMPartition->DeactivateSimpleVisSkinnedMesh(Entity, Instance, EntityManager);
					if (HolderEntity.IsValid())
					{
						DirtySkinnedHolders.Add(HolderEntity);
					}
				}

				UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
				if (Pool)
				{
					AActor* NewActor = Pool->AcquireActor(Config.ActorClass, EntityTransform, Entity);
					Instance.HydratedActor = NewActor;
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
	UE::ArcIW::SkinnedISM::FlushDirtyHolders(DirtySkinnedHolders, EntityManager);
}
