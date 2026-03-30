// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcInstancedWorld/Processors/ArcIWSimpleVisMeshActivateProcessor.h"

#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/SkinnedMeshVisualization/ArcMassSkinnedMeshFragments.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcInstancedWorld/Visualization/ArcIWSkinnedISMHelpers.h"
#include "ArcInstancedWorld/Lifecycle/ArcIWLifecycleTypes.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWSimpleVisMeshActivateProcessor)

UArcIWSimpleVisMeshActivateProcessor::UArcIWSimpleVisMeshActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWSimpleVisMeshActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::MeshCellActivated);
}

void UArcIWSimpleVisMeshActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWSimpleVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcIWLifecycleVisConfigFragment>(EMassFragmentPresence::Optional);

	SkinnedEntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	SkinnedEntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddTagRequirement<FArcIWSimpleVisSkinnedTag>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcMassSkinnedMeshFragment>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcMassVisualizationMeshConfigFragment>(EMassFragmentPresence::All);
	SkinnedEntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	SkinnedEntityQuery.AddConstSharedRequirement<FArcIWLifecycleVisConfigFragment>(EMassFragmentPresence::Optional);
}

void UArcIWSimpleVisMeshActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWSimpleVisMeshActivate);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			const TConstArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcLifecycleFragment>();
			const FArcIWLifecycleVisConfigFragment* VisConfig = Ctx.GetConstSharedFragmentPtr<FArcIWLifecycleVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (!MassISMPartition)
				{
					continue;
				}

				if (VisConfig && Config.MeshDescriptors.Num() > 0 && LifecycleFragments.Num() > 0)
				{
					const FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
					const FArcIWLifecycleMeshOverride* Override = VisConfig->FindOverride(Lifecycle.CurrentPhase);
					if (Override)
					{
						FArcIWMeshEntry EffectiveMesh = UE::ArcIW::Lifecycle::ResolvePhaseOverride(Config.MeshDescriptors[0], *Override);
						MassISMPartition->ActivateSimpleVisMesh(Entity, Instance, EffectiveMesh, EntityTransform, EntityManager, Ctx);
						continue;
					}
				}

				MassISMPartition->ActivateSimpleVisMesh(Entity, Instance, Config, EntityTransform, EntityManager, Ctx);
			}
		});

	TSet<FMassEntityHandle> DirtySkinnedHolders;
	SkinnedEntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &DirtySkinnedHolders](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();
			const FArcMassSkinnedMeshFragment& SkinnedMeshFrag = Ctx.GetConstSharedFragment<FArcMassSkinnedMeshFragment>();
			const FArcMassVisualizationMeshConfigFragment& VisMeshConfigFrag = Ctx.GetConstSharedFragment<FArcMassVisualizationMeshConfigFragment>();

			const TConstArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcLifecycleFragment>();
			const FArcIWLifecycleVisConfigFragment* VisConfig = Ctx.GetConstSharedFragmentPtr<FArcIWLifecycleVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];
				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (!MassISMPartition)
				{
					continue;
				}

				if (VisConfig && Config.MeshDescriptors.Num() > 0 && LifecycleFragments.Num() > 0)
				{
					const FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
					const FArcIWLifecycleMeshOverride* Override = VisConfig->FindOverride(Lifecycle.CurrentPhase);
					if (Override)
					{
						FArcIWMeshEntry EffectiveMesh = UE::ArcIW::Lifecycle::ResolvePhaseOverride(Config.MeshDescriptors[0], *Override);
						FMassEntityHandle HolderEntity = MassISMPartition->ActivateSimpleVisSkinnedMesh(Entity, Instance, EffectiveMesh, EntityTransform, EntityManager, Ctx);
						if (HolderEntity.IsValid())
						{
							DirtySkinnedHolders.Add(HolderEntity);
						}
						continue;
					}
				}

				FMassEntityHandle HolderEntity = MassISMPartition->ActivateSimpleVisSkinnedMesh(Entity, Instance, Config, SkinnedMeshFrag, VisMeshConfigFrag, EntityTransform, EntityManager, Ctx);
				if (HolderEntity.IsValid())
				{
					DirtySkinnedHolders.Add(HolderEntity);
				}
			}
		});
	UE::ArcIW::SkinnedISM::FlushDirtyHolders(DirtySkinnedHolders, EntityManager);
}
