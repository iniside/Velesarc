// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcAbilityGrantObserver.h"
#include "Traits/ArcAbilityGrantTrait.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcMassAbilitySet.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "MassExecutionContext.h"

UArcAbilityGrantObserver::UArcAbilityGrantObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcAbilityCollectionFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcAbilityGrantObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcAbilityCollectionFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcAbilityGrantSharedFragment>(EMassFragmentPresence::All);
}

void UArcAbilityGrantObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAbilityGrantObserver);

	ObserverQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Ctx)
	{
		const FArcAbilityGrantSharedFragment& GrantConfig = Ctx.GetConstSharedFragment<FArcAbilityGrantSharedFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

			for (const UArcMassAbilitySet* AbilitySet : GrantConfig.AbilitySets)
			{
				if (AbilitySet)
				{
					ArcAbilities::GrantAbilitySet(EntityManager, Entity, AbilitySet);
				}
			}
		}
	});
}
