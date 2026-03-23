// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWActivateProcessor.h"

#include "ArcIWTypes.h"
#include "ArcIWPartitionActor.h"
#include "ArcIWMassISMPartitionActor.h"
#include "ArcMass/ArcMassPhysicsLink.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWActivateProcessor)

// ---------------------------------------------------------------------------
// UArcIWActivateProcessor
// ---------------------------------------------------------------------------

UArcIWActivateProcessor::UArcIWActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::CellActivated);
}

void UArcIWActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	TArray<FMassEntityHandle> PhysicsLinkEntities;
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, &PhysicsLinkEntities](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcIWInstanceFragment> InstanceFragments = Ctx.GetMutableFragmentView<FArcIWInstanceFragment>();
			const FArcIWVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcIWVisConfigFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcIWInstanceFragment& Instance = InstanceFragments[EntityIt];

				if (Instance.bIsActorRepresentation)
				{
					continue;
				}

				// Skip if already has ISM instances (init observer may have added them in the same frame).
				bool bAlreadyInstanced = false;
				for (int32 Id : Instance.ISMInstanceIds)
				{
					if (Id != INDEX_NONE)
					{
						bAlreadyInstanced = true;
						break;
					}
				}
				if (bAlreadyInstanced)
				{
					continue;
				}

				AArcIWPartitionActor* Partition = Cast<AArcIWPartitionActor>(Instance.PartitionActor.Get());
				if (Partition)
				{
					const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

					Partition->AddCompositeISMInstances(
						Instance.MeshSlotBase,
						EntityTransform,
						Config.MeshDescriptors,
						Instance.ISMInstanceIds,
						Instance.InstanceIndex);
					FArcMassPhysicsLinkFragment* LinkFragment =
						EntityManager.GetFragmentDataPtr<FArcMassPhysicsLinkFragment>(Ctx.GetEntity(EntityIt));
					if (LinkFragment)
					{
						Partition->PopulatePhysicsLinkEntries(*LinkFragment, Instance.MeshSlotBase, Instance.ISMInstanceIds);
						PhysicsLinkEntities.Add(Ctx.GetEntity(EntityIt));
					}
					continue;
				}

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{
					const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
					MassISMPartition->ActivateEntity(
						Ctx.GetEntity(EntityIt), Instance, Config, EntityTransform, EntityManager);

					PhysicsLinkEntities.Add(Ctx.GetEntity(EntityIt));
				}
			}
		});

	if (PhysicsLinkEntities.Num() > 0)
	{
		UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(EntityManager.GetWorld());
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsLinkRequested, PhysicsLinkEntities);
		}
	}
}
