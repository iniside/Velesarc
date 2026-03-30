// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Observers/ArcIWSimpleVisEntityInitObserver.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/ArcIWSettings.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcInstancedWorld/Visualization/ArcIWSkinnedISMHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWSimpleVisEntityInitObserver)

UArcIWSimpleVisEntityInitObserver::UArcIWSimpleVisEntityInitObserver()
	: ObserverQuery{*this}
	, SkinnedObserverQuery{*this}
{
	ObservedTypes.Add(FArcIWSimpleVisEntityTag::StaticStruct());
	ObservedTypes.Add(FArcIWSimpleVisSkinnedTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcIWSimpleVisEntityInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcIWSimpleVisEntityTag>(EMassFragmentPresence::All);

	SkinnedObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	SkinnedObserverQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	SkinnedObserverQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	SkinnedObserverQuery.AddConstSharedRequirement<FArcMassSkinnedMeshFragment>(EMassFragmentPresence::All);
	SkinnedObserverQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	SkinnedObserverQuery.AddTagRequirement<FArcIWSimpleVisSkinnedTag>(EMassFragmentPresence::All);
}

void UArcIWSimpleVisEntityInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
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

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWSimpleVisEntityInit);

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
				// Else if cell is within mesh radius, add to ISM holder
				else if (Subsystem->IsMeshCell(Instance.MeshGridCoords))
				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						MassISMPartition->ActivateSimpleVisMesh(Entity, Instance, Config, EntityTransform, EntityManager, Ctx);

						if (Subsystem->IsPhysicsCell(Instance.PhysicsGridCoords))
						{
							PhysicsEntities.Add(Entity);
						}
					}
				}
				// Else: outside mesh radius — no ISM instance, no actor
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

	TSet<FMassEntityHandle> DirtySkinnedHolders;
	SkinnedObserverQuery.ForEachEntityChunk(Context,
		[Subsystem, &EntityManager, &DirtySkinnedHolders](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();
			const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = Ctx.GetConstSharedFragment<FArcMassSkinnedMeshFragment>();
			const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassVisualizationMeshConfigFragment>();

			TArray<FMassEntityHandle> PhysicsEntities;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				const FVector Position = EntityTransform.GetLocation();
				Instance.MeshGridCoords = Subsystem->WorldToMeshCell(Position);
				Instance.PhysicsGridCoords = Subsystem->WorldToPhysicsCell(Position);
				Instance.ActorGridCoords = Subsystem->WorldToActorCell(Position);
				Subsystem->RegisterMeshEntity(Entity, Position);
				Subsystem->RegisterPhysicsEntity(Entity, Position);
				Subsystem->RegisterActorEntity(Entity, Position);

				const bool bIsMassISM = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get()) != nullptr;
				const bool bSkipHydration = bIsMassISM && UArcIWSettings::Get()->bDisableActorHydration;

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
				else if (Subsystem->IsMeshCell(Instance.MeshGridCoords))
				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						FMassEntityHandle HolderEntity = MassISMPartition->ActivateSimpleVisSkinnedMesh(Entity, Instance, Config, SkinnedMeshFrag, VisMeshConfigFrag, EntityTransform, EntityManager, Ctx);
						if (HolderEntity.IsValid())
						{
							DirtySkinnedHolders.Add(HolderEntity);
						}

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
	UE::ArcIW::SkinnedISM::FlushDirtyHolders(DirtySkinnedHolders, EntityManager);
}
