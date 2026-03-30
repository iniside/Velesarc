// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Processors/ArcIWSimpleVisMeshDeactivateProcessor.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcInstancedWorld/Visualization/ArcIWSkinnedISMHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWSimpleVisMeshDeactivateProcessor)

UArcIWSimpleVisMeshDeactivateProcessor::UArcIWSimpleVisMeshDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWSimpleVisMeshDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::MeshCellDeactivated);
}

void UArcIWSimpleVisMeshDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FArcIWSimpleVisEntityTag>(EMassFragmentPresence::All);

	SkinnedEntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	SkinnedEntityQuery.AddTagRequirement<FArcIWSimpleVisSkinnedTag>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcMassSkinnedMeshFragment>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
}

void UArcIWSimpleVisMeshDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWSimpleVisMeshDeactivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{
					MassISMPartition->DeactivateSimpleVisMesh(Entity, Instance, EntityManager);
				}
			}
		});

	TSet<FMassEntityHandle> DirtySkinnedHolders;
	SkinnedEntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &DirtySkinnedHolders](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{
					FMassEntityHandle HolderEntity = MassISMPartition->DeactivateSimpleVisSkinnedMesh(Entity, Instance, EntityManager);
					if (HolderEntity.IsValid())
					{
						DirtySkinnedHolders.Add(HolderEntity);
					}
				}
			}
		});
	UE::ArcIW::SkinnedISM::FlushDirtyHolders(DirtySkinnedHolders, EntityManager);
}
