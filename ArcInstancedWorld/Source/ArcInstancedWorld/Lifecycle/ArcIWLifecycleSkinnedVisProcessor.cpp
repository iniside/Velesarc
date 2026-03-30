// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWLifecycleSkinnedVisProcessor.h"

#include "ArcIWLifecycleTypes.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "ArcInstancedWorld/Visualization/ArcIWSkinnedISMHelpers.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWLifecycleSkinnedVisProcessor)

UArcIWLifecycleSkinnedVisProcessor::UArcIWLifecycleSkinnedVisProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWLifecycleSkinnedVisProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::LifecyclePhaseChanged);
}

void UArcIWLifecycleSkinnedVisProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcIWLifecycleVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWSimpleVisSkinnedTag>(EMassFragmentPresence::All);
}

void UArcIWLifecycleSkinnedVisProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWLifecycleSkinnedVis);

	TSet<FMassEntityHandle> DirtySkinnedHolders;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &DirtySkinnedHolders](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FArcLifecycleFragment> LifecycleFragments = Ctx.GetFragmentView<FArcLifecycleFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();
			const FArcIWLifecycleVisConfigFragment& VisConfig = Ctx.GetConstSharedFragment<FArcIWLifecycleVisConfigFragment>();

			if (Config.MeshDescriptors.Num() == 0)
			{
				return;
			}

			const FArcIWMeshEntry& BaseMesh = Config.MeshDescriptors[0];

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];

				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (!MassISMPartition)
				{
					continue;
				}

				const FArcLifecycleFragment& Lifecycle = LifecycleFragments[EntityIt];
				const FArcIWLifecycleMeshOverride* Override = VisConfig.FindOverride(Lifecycle.CurrentPhase);

				FArcIWMeshEntry EffectiveMesh = Override
					? UE::ArcIW::Lifecycle::ResolvePhaseOverride(BaseMesh, *Override)
					: BaseMesh;

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& WorldTransform = Transforms[EntityIt].GetTransform();

				FMassEntityHandle HolderEntity = MassISMPartition->SwapSimpleVisSkinnedMesh(
					Entity, Instance, EffectiveMesh, WorldTransform, EntityManager, Ctx);

				if (HolderEntity.IsValid())
				{
					DirtySkinnedHolders.Add(HolderEntity);
				}
			}
		});

	UE::ArcIW::SkinnedISM::FlushDirtyHolders(DirtySkinnedHolders, EntityManager);
}
