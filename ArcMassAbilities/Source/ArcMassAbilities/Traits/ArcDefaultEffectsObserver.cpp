// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcDefaultEffectsObserver.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectFunctions.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "MassExecutionContext.h"

UArcDefaultEffectsObserver::UArcDefaultEffectsObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcEffectStackFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcDefaultEffectsObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FArcAggregatorFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcDefaultEffectsSharedFragment>(EMassFragmentPresence::All);
}

void UArcDefaultEffectsObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcDefaultEffectsApply);

	ObserverQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Ctx)
	{
		const FArcDefaultEffectsSharedFragment& DefaultEffects = Ctx.GetConstSharedFragment<FArcDefaultEffectsSharedFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			for (UArcEffectDefinition* Effect : DefaultEffects.DefaultEffects)
			{
				if (Effect)
				{
					ArcEffects::TryApplyEffect(EntityManager, EntityHandle, Effect, FMassEntityHandle());
				}
			}
		}
	});
}
