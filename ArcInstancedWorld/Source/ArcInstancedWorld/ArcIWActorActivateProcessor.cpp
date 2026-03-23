// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIWActorActivateProcessor.h"

#include "ArcIWTypes.h"
#include "ArcIWPartitionActor.h"
#include "ArcIWMassISMPartitionActor.h"
#include "ArcIWSettings.h"
#include "ArcIWActorPoolSubsystem.h"
#include "ArcMass/ArcMassPhysicsLink.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcIWActorActivateProcessor)

// ---------------------------------------------------------------------------
// UArcIWActorActivateProcessor — ISM -> Actor
// ---------------------------------------------------------------------------

UArcIWActorActivateProcessor::UArcIWActorActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcIWActorActivateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcIW::Signals::ActorCellActivated);
}

void UArcIWActorActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcIWInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcIWVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcIWEntityTag>(EMassFragmentPresence::All);
}

void UArcIWActorActivateProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	TArray<FMassEntityHandle> PhysicsLinkEntities;
	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, World, &PhysicsLinkEntities](FMassExecutionContext& Ctx)
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

				const bool bIsMassISM = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get()) != nullptr;
				if (bIsMassISM && UArcIWSettings::Get()->bDisableActorHydration)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();

				AArcIWPartitionActor* Partition = Cast<AArcIWPartitionActor>(Instance.PartitionActor.Get());

				// Remove ISM instances
				bool bHasISM = false;
				for (int32 Id : Instance.ISMInstanceIds)
				{
					if (Id != INDEX_NONE)
					{
						bHasISM = true;
						break;
					}
				}
				if (bHasISM && Partition)
				{
					FArcMassPhysicsLinkFragment* LinkFragment =
						EntityManager.GetFragmentDataPtr<FArcMassPhysicsLinkFragment>(Entity);
					if (LinkFragment)
					{
						AArcIWPartitionActor::DetachPhysicsLinkEntries(*LinkFragment);
					}
					Partition->RemoveCompositeISMInstances(
						Instance.MeshSlotBase,
						Config.MeshDescriptors,
						Instance.ISMInstanceIds,
						EntityManager);

					for (int32& Id : Instance.ISMInstanceIds)
					{
						Id = INDEX_NONE;
					}
				}

				AArcIWMassISMPartitionActor* MassISMPartition = Cast<AArcIWMassISMPartitionActor>(Instance.PartitionActor.Get());
				if (MassISMPartition)
				{
					// DeactivateEntity removes ISM instances + destroys physics bodies
					MassISMPartition->DeactivateEntity(Entity, Instance, Config, EntityManager);
				}

				// Acquire actor from pool
				if (Config.ActorClass)
				{
					UArcIWActorPoolSubsystem* Pool = World->GetSubsystem<UArcIWActorPoolSubsystem>();
					if (Pool)
					{
						AActor* NewActor = Pool->AcquireActor(Config.ActorClass, EntityTransform, Entity);
						Instance.HydratedActor = NewActor;
					}
				}
				Instance.bIsActorRepresentation = true;
				if (Instance.HydratedActor.IsValid())
				{
					FArcMassPhysicsLinkFragment* LinkFragment =
						EntityManager.GetFragmentDataPtr<FArcMassPhysicsLinkFragment>(Entity);
					if (LinkFragment)
					{
						ArcMassPhysicsLink::PopulateEntriesFromActor(*LinkFragment, Instance.HydratedActor.Get());
						PhysicsLinkEntities.Add(Entity);
					}
				}
			}
		});

	if (PhysicsLinkEntities.Num() > 0)
	{
		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsLinkRequested, PhysicsLinkEntities);
		}
	}
}
