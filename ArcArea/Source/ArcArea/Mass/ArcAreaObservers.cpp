// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaObservers.h"
#include "ArcAreaFragments.h"
#include "ArcAreaSubsystem.h"
#include "ArcAreaModule.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

// ====================================================================
// Add Observer
// ====================================================================

UArcAreaAddObserver::UArcAreaAddObserver()
	: ObserverQuery{*this}
{
	// Observed type must match UArcSmartObjectAddObserver's: only within one type's pipeline does
	// ExecutionPriority order observers, and a lower priority runs after it, once the handle exists.
	ObservedTypes.AddUnique(FArcSmartObjectTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
	ExecutionPriority = -10;
}

void UArcAreaAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcSmartObjectOwnerFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcAreaConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcAreaTag>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcAreaAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcAreaSubsystem* AreaSubsystem = Context.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	if (!AreaSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAreaAdd);

	ObserverQuery.ForEachEntityChunk(Context, [AreaSubsystem](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FArcSmartObjectOwnerFragment> SOOwners = Ctx.GetFragmentView<FArcSmartObjectOwnerFragment>();
		const FArcAreaConfigFragment& Config = Ctx.GetConstSharedFragment<FArcAreaConfigFragment>();
		TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();

		if (!Config.AreaDefinition.IsValid())
		{
			return;
		}

		const UArcAreaDefinition* Definition = Config.AreaDefinition.Get();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransform& Transform = TransformFragments[EntityIt].GetTransform();
			const FVector Location = Transform.GetLocation();
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			if (!AreaSubsystem->RegisterArea(Definition, Location, SOOwners[EntityIt].SmartObjectHandle, EntityHandle))
			{
				UE_LOGF(LogArcAreaDiag, Error,"Area registration failed for entity %ls using definition '%ls'.",
					*LexToString(EntityHandle), *Definition->GetName());
			}
		}
	});
}

// ====================================================================
// Remove Observer
// ====================================================================

UArcAreaRemoveObserver::UArcAreaRemoveObserver()
	: ObserverQuery{*this}
{
	// Higher priority than UArcSmartObjectRemoveObserver in the same pipeline: the area is unregistered
	// while FArcSmartObjectOwnerFragment still holds the handle it is keyed by.
	ObservedTypes.AddUnique(FArcSmartObjectTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
	ExecutionPriority = 10;
}

void UArcAreaRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcSmartObjectOwnerFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FArcAreaTag>(EMassFragmentPresence::All);
}

void UArcAreaRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAreaRemove);

	UArcAreaSubsystem* AreaSubsystem = Context.GetWorld()->GetSubsystem<UArcAreaSubsystem>();

	if (!AreaSubsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [AreaSubsystem](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FArcSmartObjectOwnerFragment> SOOwners = Ctx.GetFragmentView<FArcSmartObjectOwnerFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FSmartObjectHandle SOHandle = SOOwners[EntityIt].SmartObjectHandle;
			if (SOHandle.IsValid())
			{
				AreaSubsystem->UnregisterArea(SOHandle);
			}
		}
	});
}
