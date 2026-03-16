// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaAssignmentObserver.h"
#include "ArcAreaAssignmentFragments.h"
#include "ArcAreaSubsystem.h"
#include "MassExecutionContext.h"

UArcAreaAssignmentRemoveObserver::UArcAreaAssignmentRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.AddUnique(FArcAreaAssignmentTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcAreaAssignmentRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcAreaAssignmentFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FArcAreaAssignmentTag>(EMassFragmentPresence::All);
}

void UArcAreaAssignmentRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcAreaSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FArcAreaAssignmentFragment> AssignmentFragments = Ctx.GetFragmentView<FArcAreaAssignmentFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcAreaAssignmentFragment& Assignment = AssignmentFragments[EntityIt];
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			if (Assignment.IsAssigned())
			{
				Subsystem->UnassignEntity(EntityHandle);
			}
			Subsystem->RemoveEntityDelegates(EntityHandle);
		}
	});
}
