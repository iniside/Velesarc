// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBodyDeactivateProcessor.h"
#include "ArcMassPhysicsBody.h"
#include "ArcMassPhysicsEntityLink.h"
#include "ArcMassPhysicsSimulation.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Physics/PhysicsInterfaceCore.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassPhysicsBodyDeactivateProcessor)

UArcMassPhysicsBodyDeactivateProcessor::UArcMassPhysicsBodyDeactivateProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcMassPhysicsBodyDeactivateProcessor::InitializeInternal(
	UObject& Owner,
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::PhysicsBodyReleased);
}

void UArcMassPhysicsBodyDeactivateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcMassPhysicsBodyDeactivateProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassPhysicsBodyDeactivate);

	TArray<FBodyInstance::FAsyncTermBodyPayload> Payloads;

	EntityQuery.ForEachEntityChunk(Context,
		[&Payloads](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMassPhysicsBodyFragment> BodyFragments =
				Ctx.GetMutableFragmentView<FArcMassPhysicsBodyFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMassPhysicsBodyFragment& BodyFrag = BodyFragments[EntityIt];
				if (!BodyFrag.Body)
				{
					continue;
				}

				FBodyInstance::FAsyncTermBodyPayload Payload = BodyFrag.Body->StartAsyncTermBody_GameThread();
				if (Payload.GetPhysicsActor())
				{
					Payloads.Add(MoveTemp(Payload));
				}
				delete BodyFrag.Body;
				BodyFrag.Body = nullptr;
			}
		});

	UE::ArcMass::Physics::AsyncTermBodies(MoveTemp(Payloads));
}
