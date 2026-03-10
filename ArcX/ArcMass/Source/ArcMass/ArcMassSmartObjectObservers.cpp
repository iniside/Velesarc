// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassSmartObjectObservers.h"
#include "ArcMassSmartObjectFragments.h"
#include "MassEntityFragments.h"
#include "MassExecutionContext.h"
#include "SmartObjectSubsystem.h"

// ====================================================================
// Add Observer
// ====================================================================

UArcSmartObjectAddObserver::UArcSmartObjectAddObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcSmartObjectTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcSmartObjectAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcSmartObjectOwnerFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddConstSharedRequirement<FArcSmartObjectDefinitionSharedFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FArcSmartObjectTag>(EMassFragmentPresence::All);
}

void UArcSmartObjectAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [SOSubsystem](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcSmartObjectOwnerFragment> SOOwners = Ctx.GetMutableFragmentView<FArcSmartObjectOwnerFragment>();
		const FArcSmartObjectDefinitionSharedFragment& SODef = Ctx.GetConstSharedFragment<FArcSmartObjectDefinitionSharedFragment>();
		TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

		if (!SODef.SmartObjectDefinition)
		{
			return;
		}

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransform& Transform = Transforms[EntityIt].GetTransform();
			const FMassEntityHandle EntityHandle = Ctx.GetEntity(EntityIt);

			SOOwners[EntityIt].SmartObjectHandle = SOSubsystem->CreateSmartObject(
				*SODef.SmartObjectDefinition, Transform, FConstStructView::Make(EntityHandle));
		}
	});
}

// ====================================================================
// Remove Observer
// ====================================================================

UArcSmartObjectRemoveObserver::UArcSmartObjectRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcSmartObjectTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcSmartObjectRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcSmartObjectOwnerFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddTagRequirement<FArcSmartObjectTag>(EMassFragmentPresence::All);
}

void UArcSmartObjectRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();
	if (!SOSubsystem)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context, [SOSubsystem](FMassExecutionContext& Ctx)
	{
		TArrayView<FArcSmartObjectOwnerFragment> SOOwners = Ctx.GetMutableFragmentView<FArcSmartObjectOwnerFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcSmartObjectOwnerFragment& SOOwner = SOOwners[EntityIt];
			if (SOOwner.SmartObjectHandle.IsValid())
			{
				SOSubsystem->DestroySmartObject(SOOwner.SmartObjectHandle);
				SOOwner.SmartObjectHandle = FSmartObjectHandle();
			}
		}
	});
}
