// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Processors/ArcIWSimpleVisActorDeactivateProcessor.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/Visualization/ArcIWVisualizationSubsystem.h"
#include "ArcInstancedWorld/ArcIWActorPoolSubsystem.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcInstancedWorld/Visualization/ArcIWSkinnedISMHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWSimpleVisActorDeactivateProcessor)

UArcIWSimpleVisActorDeactivateProcessor::UArcIWSimpleVisActorDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWSimpleVisActorDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::ActorCellDeactivated);
}

void UArcIWSimpleVisActorDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
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

void UArcIWSimpleVisActorDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWSimpleVisActorDeactivate);

	UArcIWVisualizationSubsystem* VisSubsystem = World->GetSubsystem<UArcIWVisualizationSubsystem>();

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World, VisSubsystem](FMassExecutionContext& Ctx)
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

				if (Instance.bKeepHydrated)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				// Release hydrated actor back to pool
				AActor* Actor = Instance.HydratedActor.Get();
				if (Actor)
				{
					UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
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

				// Restore mesh via ISM holder if cell is within mesh radius
				bool bShowMesh = VisSubsystem && VisSubsystem->IsMeshCell(Instance.MeshGridCoords);
				if (bShowMesh)
				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						MassISMPartition->ActivateSimpleVisMesh(Entity, Instance, Config, EntityTransform, EntityManager, Ctx);

						// Re-activate physics if within physics radius
						if (VisSubsystem->IsPhysicsCell(Instance.PhysicsGridCoords))
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

	TSet<FMassEntityHandle> DirtySkinnedHolders;
	SkinnedEntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World, VisSubsystem, &DirtySkinnedHolders](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();
			const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = Ctx.GetConstSharedFragment<FArcMassSkinnedMeshFragment>();
			const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassVisualizationMeshConfigFragment>();

			TArray<FMassEntityHandle> PhysicsEntities;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				if (!Instance.bIsActorRepresentation)
				{
					continue;
				}

				if (Instance.bKeepHydrated)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				AActor* Actor = Instance.HydratedActor.Get();
				if (Actor)
				{
					UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
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

				bool bShowMesh = VisSubsystem && VisSubsystem->IsMeshCell(Instance.MeshGridCoords);
				if (bShowMesh)
				{
					AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
					if (MassISMPartition)
					{
						FMassEntityHandle HolderEntity = MassISMPartition->ActivateSimpleVisSkinnedMesh(Entity, Instance, Config, SkinnedMeshFrag, VisMeshConfigFrag, EntityTransform, EntityManager, Ctx);
						if (HolderEntity.IsValid())
						{
							DirtySkinnedHolders.Add(HolderEntity);
						}

						if (VisSubsystem->IsPhysicsCell(Instance.PhysicsGridCoords))
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
