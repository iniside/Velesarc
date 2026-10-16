// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActionProcessor.h"

#include "ArcMassAction.h"
#include "ArcMassActionFragment.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassActionProcessor)

UArcMassDeathActionProcessor::UArcMassDeathActionProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
}

void UArcMassDeathActionProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::LifecycleDead);
}

void UArcMassDeathActionProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddSparseRequirement<FArcMassActionFragment>(EMassFragmentPresence::All);
}

void UArcMassDeathActionProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassDeathActions);

	const FName SignalName = UE::ArcMass::Signals::LifecycleDead;

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, World, SignalName](FMassExecutionContext& Ctx)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			FStructView SparseView = EntityManager.GetMutableSparseElementDataForEntity(FArcMassActionFragment::StaticStruct(), Entity);
			FArcMassActionFragment& Fragment = SparseView.Get<FArcMassActionFragment>();

			FArcMassActionContext ActionContext{EntityManager, Ctx, *World, Entity, FConstStructView()};
			Fragment.Execute(SignalName, ActionContext);
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			FStructView SparseView = EntityManager.GetMutableSparseElementDataForEntity(FArcMassActionFragment::StaticStruct(), Entity);
			FArcMassActionFragment& Fragment = SparseView.Get<FArcMassActionFragment>();

			FArcMassActionContext ActionContext{EntityManager, Ctx, *World, Entity, FConstStructView()};
			Fragment.PostExecute(SignalName, ActionContext);
		}
	});
}
