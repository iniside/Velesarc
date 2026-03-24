// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWPhysicsDeactivateProcessor.h"

#include "ArcIWTypes.h"
#include "ArcIWMassISMPartitionActor.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWPhysicsDeactivateProcessor)

// ---------------------------------------------------------------------------
// UArcIWPhysicsDeactivateProcessor
// ---------------------------------------------------------------------------

UArcIWPhysicsDeactivateProcessor::UArcIWPhysicsDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWPhysicsDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::PhysicsCellDeactivated);
}

void UArcIWPhysicsDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWPhysicsDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
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

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (!MassISMPartition)
				{
					continue;
				}

				MassISMPartition->DeactivatePhysics(Ctx.GetEntity(EntityIt), EntityManager);
			}
		});
}
