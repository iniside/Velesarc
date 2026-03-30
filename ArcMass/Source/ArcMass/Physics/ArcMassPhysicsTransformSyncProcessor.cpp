// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsTransformSyncProcessor.h"
#include "ArcMassPhysicsBody.h"
#include "ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "PhysicsEngine/BodyInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassPhysicsTransformSyncProcessor)

// ---------------------------------------------------------------------------

UArcMassPhysicsTransformSyncProcessor::UArcMassPhysicsTransformSyncProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

// ---------------------------------------------------------------------------

void UArcMassPhysicsTransformSyncProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddSparseRequirement<FArcMassPhysicsSimulatingTag>(EMassFragmentPresence::All);
}

// ---------------------------------------------------------------------------

void UArcMassPhysicsTransformSyncProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	UMassSignalSubsystem* SignalSubsystem = Context.GetWorld()->GetSubsystem<UMassSignalSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassPhysicsTransformSync);

	TArray<FMassEntityHandle> EntitiesToStopSimulating;
	TArray<FMassEntityHandle> EntitiesToSignal;
	TArray<FMassEntityHandle> EntitiesToSleep;

	EntityQuery.ForEachEntityChunk(Context,
		[&EntitiesToStopSimulating, &EntitiesToSignal, &EntitiesToSleep](FMassExecutionContext& Ctx)
		{
			TArrayView<FTransformFragment> Transforms = Ctx.GetMutableFragmentView<FTransformFragment>();
			TConstArrayView<FArcMassPhysicsBodyFragment> Bodies = Ctx.GetFragmentView<FArcMassPhysicsBodyFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMassPhysicsBodyFragment& BodyFrag = Bodies[EntityIt];
				if (!BodyFrag.Body)
				{
					continue;
				}

				FTransform PhysicsTransform = BodyFrag.Body->GetUnrealWorldTransform();
				PhysicsTransform.SetScale3D(BodyFrag.Body->Scale3D);
				Transforms[EntityIt].GetMutableTransform() = PhysicsTransform;

				FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
				EntitiesToSignal.Add(Entity);

				if (!BodyFrag.Body->IsInstanceAwake())
				{
					BodyFrag.Body->SetInstanceSimulatePhysics(false);
					EntitiesToStopSimulating.Add(Entity);
					EntitiesToSleep.Add(Entity);
				}
			}
		});

	if (EntitiesToStopSimulating.Num() > 0)
	{
		EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Remove>>([Entities = MoveTemp(EntitiesToStopSimulating)](FMassEntityManager& Mgr)
		{
				for (const FMassEntityHandle& Entity : Entities)
				{
					Mgr.RemoveSparseElementFromEntity<FArcMassPhysicsSimulatingTag>(Entity);
				}
		});
	}

	// Signal all updated entities for ISM visual sync
	if (SignalSubsystem && EntitiesToSignal.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsTransformUpdated, EntitiesToSignal);
	}

	// Signal sleeping entities for full bounds recalculation
	if (SignalSubsystem && EntitiesToSleep.Num() > 0)
	{
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::PhysicsBodySlept, EntitiesToSleep);
	}
}
