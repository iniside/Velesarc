// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBodyAttachProcessor.h"
#include "ArcMassPhysicsBody.h"
#include "ArcMassPhysicsLink.h"
#include "ArcMassPhysicsEntityLink.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "PhysicsEngine/BodyInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassPhysicsBodyAttachProcessor)

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UArcMassPhysicsBodyAttachProcessor::UArcMassPhysicsBodyAttachProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

// ---------------------------------------------------------------------------
// InitializeInternal
// ---------------------------------------------------------------------------

void UArcMassPhysicsBodyAttachProcessor::InitializeInternal(
	UObject& Owner,
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::PhysicsLinkRequested);
}

// ---------------------------------------------------------------------------
// ConfigureQueries
// ---------------------------------------------------------------------------

void UArcMassPhysicsBodyAttachProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadOnly);
}

// ---------------------------------------------------------------------------
// SignalEntities
// ---------------------------------------------------------------------------

void UArcMassPhysicsBodyAttachProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context,
	FMassSignalNameLookup& EntitySignals)
{
	TArray<FMassEntityHandle> EntitiesToReSignal;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntitiesToReSignal](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FArcMassPhysicsBodyFragment> BodyFragments =
				Ctx.GetFragmentView<FArcMassPhysicsBodyFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMassPhysicsBodyFragment& Fragment = BodyFragments[EntityIt];
				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				bool bAllBodiesValid = true;

				for (FBodyInstance* Body : Fragment.Bodies)
				{
					if (!Body)
					{
						continue;
					}

					if (!Body->GetPhysicsActor())
					{
						bAllBodiesValid = false;
						break;
					}
				}

				if (bAllBodiesValid)
				{
					for (FBodyInstance* Body : Fragment.Bodies)
					{
						if (!Body)
						{
							continue;
						}

						ArcMassPhysicsEntityLink::Attach(*Body, Entity, nullptr);
					}
				}
				else
				{
					EntitiesToReSignal.Add(Entity);
				}
			}
		});

	if (EntitiesToReSignal.Num() > 0)
	{
		UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();
		if (SignalSubsystem)
		{
			SignalSubsystem->SignalEntities(
				UE::ArcMass::Signals::PhysicsLinkRequested,
				EntitiesToReSignal);
		}
	}
}
