// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaObservers.h"
#include "ArcAreaFragments.h"
#include "ArcAreaSubsystem.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectObservers.h"
#include "Mass/EntityFragments.h"
#include "MassExecutionContext.h"

// ====================================================================
// Add Observer
// ====================================================================

UArcAreaAddObserver::UArcAreaAddObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.AddUnique(FArcAreaTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteAfter.Add(UArcSmartObjectAddObserver::StaticClass()->GetFName());
}

void UArcAreaAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcAreaFragment>(EMassFragmentAccess::ReadWrite);
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

	ObserverQuery.ForEachEntityChunk(Context, [AreaSubsystem, &EntityManager](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcAreaFragment> AreaFragments = Ctx.GetMutableFragmentView<FArcAreaFragment>();
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

			// Read SmartObject handle if this entity owns one (created by UArcSmartObjectAddObserver).
			FSmartObjectHandle SOHandle;
			const FArcSmartObjectOwnerFragment* SOOwner = EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(EntityHandle);
			if (SOOwner)
			{
				SOHandle = SOOwner->SmartObjectHandle;
			}

			const FArcAreaHandle AreaHandle = AreaSubsystem->RegisterArea(Definition, Location, SOHandle, EntityHandle);
			AreaFragments[EntityIt].AreaHandle = AreaHandle;
		}
	});
}

// ====================================================================
// Remove Observer
// ====================================================================

UArcAreaRemoveObserver::UArcAreaRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.AddUnique(FArcAreaTag::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteBefore.Add(UArcSmartObjectRemoveObserver::StaticClass()->GetFName());
}

void UArcAreaRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcAreaFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FArcAreaTag>(EMassFragmentPresence::All);
}

void UArcAreaRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcAreaRemove);

	UArcAreaSubsystem* AreaSubsystem = Context.GetWorld()->GetSubsystem<UArcAreaSubsystem>();

	ObserverQuery.ForEachEntityChunk(Context, [AreaSubsystem](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FArcAreaFragment> AreaFragments = Ctx.GetFragmentView<FArcAreaFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcAreaHandle AreaHandle = AreaFragments[EntityIt].AreaHandle;
			if (AreaHandle.IsValid() && AreaSubsystem)
			{
				AreaSubsystem->UnregisterArea(AreaHandle);
			}
		}
	});
}
