// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassActionTrait.h"

#include "ArcMassActionAsset.h"
#include "ArcMassActionFragment.h"
#include "MassCommandBuffer.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassActionTrait)

// ---------------------------------------------------------------------------
// Trait
// ---------------------------------------------------------------------------

void UArcMassActionTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddTag<FArcMassActionTag>();

	FArcMassActionConfigFragment Config;
	Config.ActionMap = ActionMap;

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(Config);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}

// ---------------------------------------------------------------------------
// Init Observer
// ---------------------------------------------------------------------------

UArcMassActionInitObserver::UArcMassActionInitObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcMassActionTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcMassActionInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddTagRequirement<FArcMassActionTag>(EMassFragmentPresence::All);
	ObserverQuery.AddConstSharedRequirement<FArcMassActionConfigFragment>();
}

void UArcMassActionInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context, [&EntityManager](FMassExecutionContext& Ctx)
	{
		const FArcMassActionConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMassActionConfigFragment>();
		TMap<FName, FArcMassActionTraitEntry> ActionMap = Config.ActionMap;

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

			EntityManager.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Add>>(
				[Entity, ActionMap](FMassEntityManager& Mgr)
				{
					FArcMassActionFragment& Fragment = Mgr.AddSparseElementToEntity<FArcMassActionFragment>(Entity);

					for (const TPair<FName, FArcMassActionTraitEntry>& Pair : ActionMap)
					{
						if (Pair.Value.StatelessActions)
						{
							Fragment.StatelessActions.Add(Pair.Key, Pair.Value.StatelessActions);
						}

						if (Pair.Value.StatefulActions)
						{
							FArcMassStatefulActionList& List = Fragment.StatefulActions.FindOrAdd(Pair.Key);
							List.Actions = Pair.Value.StatefulActions->Actions;
						}
					}
				});
		}
	});
}
