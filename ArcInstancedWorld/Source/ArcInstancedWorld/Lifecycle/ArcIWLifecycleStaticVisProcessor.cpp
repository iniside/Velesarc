// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWLifecycleStaticVisProcessor.h"

#include "ArcIWLifecycleTypes.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcInstancedWorld/Visualization/ArcIWMassISMPartitionActor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWLifecycleStaticVisProcessor)

UArcIWLifecycleStaticVisProcessor::UArcIWLifecycleStaticVisProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWLifecycleStaticVisProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::LifecyclePhaseChanged);
}

void UArcIWLifecycleStaticVisProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcLifecycleFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcIWLifecycleVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWSimpleVisEntityTag>(EMassFragmentPresence::All);
}

void UArcIWLifecycleStaticVisProcessor::SignalEntities(FMassEntityManager& EntityManager,
	FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIWLifecycleStaticVis);

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager](FMassExecutionContext& Ctx)
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

				MassISMPartition->SwapSimpleVisMesh(Entity, Instance, EffectiveMesh, WorldTransform, EntityManager, Ctx);
			}
		});
}
