// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcEffectRemovalObserver.h"

#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Mass/ExecutionContext.h"

UArcEffectRemovalObserver::UArcEffectRemovalObserver()
{
	ObservedTypes.Add(FArcEffectStackFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcEffectRemovalObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
}

void UArcEffectRemovalObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TArray<TPair<FMassEntityHandle, TArray<FArcActiveEffect>>> RemovedEffects;

	ObserverQuery.ForEachEntityChunk(Context, [&RemovedEffects](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcEffectStackFragment> EffectStacks = Ctx.GetMutableFragmentView<FArcEffectStackFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcEffectStackFragment& EffectStack = EffectStacks[EntityIt];
			if (EffectStack.ActiveEffects.IsEmpty())
			{
				continue;
			}
			RemovedEffects.Emplace(Ctx.GetEntity(EntityIt), MoveTemp(EffectStack.ActiveEffects));
		}
	});

	for (TPair<FMassEntityHandle, TArray<FArcActiveEffect>>& Entry : RemovedEffects)
	{
		for (FArcActiveEffect& ActiveEffect : Entry.Value)
		{
			ArcEffects::CleanupActiveEffect(EntityManager, Entry.Key, MoveTemp(ActiveEffect), EArcEffectRemovalReason::EntityDestroyed);
		}
	}
}
