// Copyright Lukasz Baran. All Rights Reserved.

#include "Traits/ArcMassItemStoreTrait.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "MassCommandBuffer.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"

void UArcMassItemStoreTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FArcMassItemStoreTag>();
}

UArcMassItemStoreInitObserver::UArcMassItemStoreInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassItemStoreTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcMassItemStoreInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddTagRequirement<FArcMassItemStoreTag>(EMassFragmentPresence::All);
}

void UArcMassItemStoreInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			Ctx.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>(
				[Entity](FMassEntityManager& Mgr)
				{
					Mgr.AddSparseElementToEntity<FArcMassItemStoreFragment>(Entity);
				});
		}
	});
}
