// Copyright Lukasz Baran. All Rights Reserved.

#include "QuickBar/ArcMassQuickBarTrait.h"
#include "QuickBar/ArcMassQuickBarConfig.h"
#include "QuickBar/ArcMassQuickBarSharedFragment.h"
#include "QuickBar/ArcMassQuickBarStateFragment.h"
#include "MassCommandBuffer.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"

void UArcMassQuickBarTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FArcMassQuickBarTag>();

	FArcMassQuickBarSharedFragment SharedFragment;
	SharedFragment.Config = QuickBarConfig;
	BuildContext.AddConstSharedFragment(FConstSharedStruct::Make(SharedFragment));
}

UArcMassQuickBarStateInitObserver::UArcMassQuickBarStateInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassQuickBarTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcMassQuickBarStateInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddTagRequirement<FArcMassQuickBarTag>(EMassFragmentPresence::All);
}

void UArcMassQuickBarStateInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context, [](FMassExecutionContext& Ctx)
	{
		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			Ctx.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>(
				[Entity](FMassEntityManager& Mgr)
				{
					Mgr.AddSparseElementToEntity<FArcMassQuickBarStateFragment>(Entity);
				});
		}
	});
}
