// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassHealthSignalProcessor.h"
#include "ArcMassFragments.h"
#include "MassActorSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassExecutionContext.h"

UArcMassHealthSignalProcessor::UArcMassHealthSignalProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
}

void UArcMassHealthSignalProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(Owner.GetWorld());
	SubscribeToSignal(*SignalSubsystem, UE::ArcMass::Signals::HealthChanged);
}

void UArcMassHealthSignalProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMassHealthFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMassLastUpdateTimeFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcMassHealthSignalProcessor::SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals)
{
	const UWorld* World = EntityManager.GetWorld();
	const double CurrentTime = World->GetTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [CurrentTime](FMassExecutionContext& Context)
	{
		const TConstArrayView<FArcMassHealthFragment> HealthFragments = Context.GetFragmentView<FArcMassHealthFragment>();
		const TArrayView<FMassActorFragment> ActorFragments = Context.GetMutableFragmentView<FMassActorFragment>();
		TArrayView<FArcMassLastUpdateTimeFragment> LastUpdateFragments = Context.GetMutableFragmentView<FArcMassLastUpdateTimeFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassActorFragment& ActorFragment = ActorFragments[EntityIt];
			AActor* Actor = ActorFragment.GetMutable();
			if (!Actor)
			{
				continue;
			}

			if (Actor->GetClass()->ImplementsInterface(UArcMassHealthObserverInterface::StaticClass()))
			{
				const FArcMassHealthFragment& Health = HealthFragments[EntityIt];
				FArcMassLastUpdateTimeFragment& LastUpdate = LastUpdateFragments[EntityIt];

				IArcMassHealthObserverInterface::Execute_OnHealthUpdated(Actor, Actor, Health.CurrentHealth, Health.MaxHealth, CurrentTime, LastUpdate.LastUpdateTime);
				LastUpdate.LastUpdateTime = CurrentTime;
			}
		}
	});
}
