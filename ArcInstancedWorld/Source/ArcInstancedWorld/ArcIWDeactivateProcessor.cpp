// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWDeactivateProcessor.h"

#include "ArcIWTypes.h"
#include "ArcIWPartitionActor.h"
#include "ArcIWMassISMPartitionActor.h"
#include "ArcMass/ArcMassPhysicsLink.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWDeactivateProcessor)

// ---------------------------------------------------------------------------
// UArcIWDeactivateProcessor
// ---------------------------------------------------------------------------

UArcIWDeactivateProcessor::UArcIWDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWDeactivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::CellDeactivated);
}

void UArcIWDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWDeactivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];

				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				AArcIWPartitionActor* Partition = Cast<AArcIWPartitionActor>(Instance.PartitionActor.Get());
				if (Partition)
				{
					FArcMassPhysicsLinkFragment* LinkFragment =
						EntityManager.GetFragmentDataPtr<FArcMassPhysicsLinkFragment>(Ctx.GetEntity(EntityIt));
					if (LinkFragment)
					{
						AArcIWPartitionActor::DetachPhysicsLinkEntries(*LinkFragment);
					}
					Partition->RemoveCompositeISMInstances(
						Instance.MeshSlotBase,
						Config.MeshDescriptors,
						Instance.ISMInstanceIds,
						EntityManager);

					for (int32& InstanceId : Instance.ISMInstanceIds)
					{
						InstanceId = INDEX_NONE;
					}
					continue;
				}

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{
					MassISMPartition->DeactivateEntity(
						Ctx.GetEntity(EntityIt), Instance, Config, EntityManager);
				}

				for (int32& InstanceId : Instance.ISMInstanceIds)
				{
					InstanceId = INDEX_NONE;
				}
			}
		});
}
