// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAreaObservers.h"
#include "ArcAreaFragments.h"
#include "ArcAreaSubsystem.h"
#include "ArcMass/ArcMassSmartObjectFragments.h"
#include "MassEntityFragments.h"
#include "MassExecutionContext.h"
#include "SmartObjectSubsystem.h"
#include "ArcMass/ArcMassSmartObjectFragments.h"

// ====================================================================
// Add Observer
// ====================================================================

UArcAreaAddObserver::UArcAreaAddObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcAreaTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
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

	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();

	ObserverQuery.ForEachEntityChunk(Context, [AreaSubsystem, SOSubsystem, &EntityManager](FMassExecutionContext& Ctx)
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

			FSmartObjectHandle SOHandle;

			// Create SmartObject if this entity has the owner fragment and a definition.
			FArcSmartObjectOwnerFragment* SOOwner = EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(EntityHandle);
			if (SOOwner && SOSubsystem)
			{
				const FArcSmartObjectDefinitionSharedFragment* SODef = EntityManager.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(EntityHandle);
				if (SODef && SODef->SmartObjectDefinition)
				{
					SOOwner->SmartObjectHandle = SOSubsystem->CreateSmartObject(
						*SODef->SmartObjectDefinition, Transform, FConstStructView::Make(EntityHandle));
					SOHandle = SOOwner->SmartObjectHandle;
				}
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
	ObservedType = FArcAreaTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcAreaRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcAreaFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FArcAreaTag>(EMassFragmentPresence::All);
}

void UArcAreaRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcAreaSubsystem* AreaSubsystem = Context.GetWorld()->GetSubsystem<UArcAreaSubsystem>();
	USmartObjectSubsystem* SOSubsystem = Context.GetWorld()->GetSubsystem<USmartObjectSubsystem>();

	ObserverQuery.ForEachEntityChunk(Context, [AreaSubsystem, SOSubsystem, &EntityManager](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FArcAreaFragment> AreaFragments = Ctx.GetFragmentView<FArcAreaFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcAreaHandle AreaHandle = AreaFragments[EntityIt].AreaHandle;
			if (AreaHandle.IsValid() && AreaSubsystem)
			{
				AreaSubsystem->UnregisterArea(AreaHandle);
			}

			// Destroy SmartObject if this entity owns one.
			FArcSmartObjectOwnerFragment* SOOwner = EntityManager.GetFragmentDataPtr<FArcSmartObjectOwnerFragment>(Ctx.GetEntity(EntityIt));
			if (SOOwner && SOOwner->SmartObjectHandle.IsValid() && SOSubsystem)
			{
				SOSubsystem->DestroySmartObject(SOOwner->SmartObjectHandle);
				SOOwner->SmartObjectHandle = FSmartObjectHandle();
			}
		}
	});
}
