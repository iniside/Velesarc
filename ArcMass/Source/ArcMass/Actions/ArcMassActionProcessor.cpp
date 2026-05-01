// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActionProcessor.h"

#include "ArcMassAction.h"
#include "ArcMassActionAsset.h"
#include "ArcMassActionFragment.h"
#include "ArcMass/Lifecycle/ArcMassLifecycle.h"
#include "MassExecutionContext.h"
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
		// --- Pass 1: Execute ---
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			FStructView SparseView = EntityManager.GetMutableSparseElementDataForEntity(FArcMassActionFragment::StaticStruct(), Entity);
			FArcMassActionFragment& Fragment = SparseView.Get<FArcMassActionFragment>();

			FArcMassActionContext ActionContext{EntityManager, Ctx, *World, Entity};

			// Stateless first
			TObjectPtr<UArcMassStatelessActionAsset>* StatelessAsset = Fragment.StatelessActions.Find(SignalName);
			if (StatelessAsset && *StatelessAsset)
			{
				for (const FInstancedStruct& ActionStruct : (*StatelessAsset)->Actions)
				{
					if (const FArcMassStatelessAction* Action = ActionStruct.GetPtr<FArcMassStatelessAction>())
					{
						Action->Execute(ActionContext);
					}
				}
			}

			// Stateful second
			FArcMassStatefulActionList* StatefulList = Fragment.StatefulActions.Find(SignalName);
			if (StatefulList)
			{
				for (FInstancedStruct& ActionStruct : StatefulList->Actions)
				{
					if (FArcMassStatefulAction* Action = ActionStruct.GetMutablePtr<FArcMassStatefulAction>())
					{
						Action->Execute(ActionContext);
					}
				}
			}
		}

		// --- Pass 2: PostExecute ---
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			FStructView SparseView = EntityManager.GetMutableSparseElementDataForEntity(FArcMassActionFragment::StaticStruct(), Entity);
			FArcMassActionFragment& Fragment = SparseView.Get<FArcMassActionFragment>();

			FArcMassActionContext ActionContext{EntityManager, Ctx, *World, Entity};

			// Stateless first
			TObjectPtr<UArcMassStatelessActionAsset>* StatelessAsset = Fragment.StatelessActions.Find(SignalName);
			if (StatelessAsset && *StatelessAsset)
			{
				for (const FInstancedStruct& ActionStruct : (*StatelessAsset)->Actions)
				{
					if (const FArcMassStatelessAction* Action = ActionStruct.GetPtr<FArcMassStatelessAction>())
					{
						Action->PostExecute(ActionContext);
					}
				}
			}

			// Stateful second
			FArcMassStatefulActionList* StatefulList = Fragment.StatefulActions.Find(SignalName);
			if (StatefulList)
			{
				for (FInstancedStruct& ActionStruct : StatefulList->Actions)
				{
					if (FArcMassStatefulAction* Action = ActionStruct.GetMutablePtr<FArcMassStatefulAction>())
					{
						Action->PostExecute(ActionContext);
					}
				}
			}
		}
	});
}
