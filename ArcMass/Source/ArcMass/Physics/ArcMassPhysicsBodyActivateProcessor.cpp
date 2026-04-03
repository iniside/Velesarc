// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBodyActivateProcessor.h"
#include "ArcMassPhysicsBody.h"
#include "ArcMassPhysicsBodyConfig.h"
#include "ArcMassPhysicsEntityLink.h"
#include "ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/PhysicsInterfaceCore.h"
#include "Chaos/ChaosUserEntity.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassPhysicsBodyActivateProcessor)

UArcMassPhysicsBodyActivateProcessor::UArcMassPhysicsBodyActivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcMassPhysicsBodyActivateProcessor::InitializeInternal(
	UObject& Owner,
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::PhysicsBodyRequested);
}

void UArcMassPhysicsBodyActivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddConstSharedRequirement<FArcMassPhysicsBodyConfigFragment>(EMassFragmentPresence::All);
}

void UArcMassPhysicsBodyActivateProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassPhysicsBodyActivate);

	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	FPhysScene* PhysScene = World->GetPhysicsScene();
	if (!PhysScene)
	{
		return;
	}

	TArray<FMassEntityHandle> EntitiesToReSignal;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntityManager, PhysScene, &EntitiesToReSignal](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcMassPhysicsBodyFragment> BodyFragments = Ctx.GetMutableFragmentView<FArcMassPhysicsBodyFragment>();

			const FArcMassPhysicsBodyConfigFragment& PhysicsConfig =
				Ctx.GetConstSharedFragment<FArcMassPhysicsBodyConfigFragment>();

			if (!PhysicsConfig.BodySetup)
			{
				return;
			}

			TArray<FTransform> BatchTransforms;
			TArray<int32> BatchIndices;
			BatchTransforms.Reserve(Ctx.GetNumEntities());
			BatchIndices.Reserve(Ctx.GetNumEntities());

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMassPhysicsBodyFragment& BodyFrag = BodyFragments[EntityIt];
				if (BodyFrag.Body)
				{
					continue;
				}

				BatchTransforms.Add(Transforms[EntityIt].GetTransform());
				BatchIndices.Add(*EntityIt);
			}

			if (BatchTransforms.Num() == 0)
			{
				return;
			}

			TArray<FBodyInstance*> Bodies;
			UE::ArcMass::Physics::InitBodiesFromConfig(PhysicsConfig, BatchTransforms, PhysScene, Bodies);

			for (int32 BatchIdx = 0; BatchIdx < BatchIndices.Num(); ++BatchIdx)
			{
				int32 EntityIdx = BatchIndices[BatchIdx];
				FBodyInstance* Body = Bodies.IsValidIndex(BatchIdx) ? Bodies[BatchIdx] : nullptr;
				if (Body)
				{
					BodyFragments[EntityIdx].Body = Body;
				}
			}

			// Attach entity links for all entities with bodies.
			// Handles both freshly created bodies and re-signaled entities
			// whose Chaos actor wasn't ready on the previous frame.
			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMassPhysicsBodyFragment& BodyFrag = BodyFragments[EntityIt];
				if (!BodyFrag.Body)
				{
					continue;
				}

				FPhysicsActorHandle ActorHandle = BodyFrag.Body->GetPhysicsActor();
				if (!ActorHandle)
				{
					EntitiesToReSignal.Add(Ctx.GetEntity(EntityIt));
					continue;
				}

				// Skip if already attached
				void* UserData = ActorHandle->GetGameThreadAPI().UserData();
				if (FChaosUserData::Get<FChaosUserEntityAppend>(UserData))
				{
					continue;
				}

				ArcMassPhysicsEntityLink::Attach(*BodyFrag.Body, Ctx.GetEntity(EntityIt));
			}
		});

	if (EntitiesToReSignal.Num() > 0)
	{
		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodyRequested, EntitiesToReSignal);
		}
	}
}
