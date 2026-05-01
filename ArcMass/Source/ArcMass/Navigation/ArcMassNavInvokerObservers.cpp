// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Navigation/ArcMassNavInvokerObservers.h"

#include "MassExecutionContext.h"
#include "Mass/EntityFragments.h"
#include "ArcMass/Navigation/ArcMassNavInvokerTypes.h"
#include "ArcMass/Navigation/ArcMassNavInvokerSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassNavInvokerObservers)

namespace ArcMassNavInvokerObserversInternal
{
	/**
	 * Shared registration logic: build a FArcNavInvokerData from the fragment and
	 * entity transform, allocate a slot in the subsystem, and write the resulting
	 * InvokerIndex and LastCell back into the fragment.
	 */
	static void RegisterInvoker(
		FMassNavInvokerFragment& Fragment,
		const FVector& Location,
		UArcMassNavInvokerSubsystem& Subsystem)
	{
		FArcNavInvokerData InvokerData;
		InvokerData.Location = Location;
		InvokerData.RadiusMin = Fragment.GenerationRadius;
		InvokerData.RadiusMax = Fragment.RemovalRadius;
		InvokerData.SupportedAgents = Fragment.SupportedAgents;
		InvokerData.Priority = Fragment.Priority;

		Fragment.InvokerIndex = Subsystem.AllocateSlot(InvokerData);
		Fragment.CachedNavmeshOrigin = Subsystem.GetNavmeshOrigin();
		Fragment.CachedTileDim = Subsystem.GetTileDim();
		Fragment.LastCell = ArcNavInvoker::WorldToCell(Location, Subsystem.GetCellSize());
	}
} // namespace ArcMassNavInvokerObserversInternal

// ============================================================================
// UArcMassNavInvokerMoveableObserver
// ============================================================================

UArcMassNavInvokerMoveableObserver::UArcMassNavInvokerMoveableObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FMassNavInvokerFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcMassNavInvokerMoveableObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FMassNavInvokerFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FMassNavInvokerStaticTag>(EMassFragmentPresence::None);
}

void UArcMassNavInvokerMoveableObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcMassNavInvokerSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcMassNavInvokerSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNavInvokerMoveableAdd);

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& Ctx)
	{
		TArrayView<FMassNavInvokerFragment> Invokers = Ctx.GetMutableFragmentView<FMassNavInvokerFragment>();
		TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcMassNavInvokerObserversInternal::RegisterInvoker(
				Invokers[EntityIt],
				Transforms[EntityIt].GetTransform().GetLocation(),
				*Subsystem);
		}
	});
}

// ============================================================================
// UArcMassNavInvokerStaticObserver
// ============================================================================

UArcMassNavInvokerStaticObserver::UArcMassNavInvokerStaticObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FMassNavInvokerFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcMassNavInvokerStaticObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FMassNavInvokerFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddTagRequirement<FMassNavInvokerStaticTag>(EMassFragmentPresence::All);
}

void UArcMassNavInvokerStaticObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcMassNavInvokerSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcMassNavInvokerSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNavInvokerStaticAdd);

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& Ctx)
	{
		TArrayView<FMassNavInvokerFragment> Invokers = Ctx.GetMutableFragmentView<FMassNavInvokerFragment>();
		TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			ArcMassNavInvokerObserversInternal::RegisterInvoker(
				Invokers[EntityIt],
				Transforms[EntityIt].GetTransform().GetLocation(),
				*Subsystem);
		}
	});
}

// ============================================================================
// UArcMassNavInvokerRemovalObserver
// ============================================================================

UArcMassNavInvokerRemovalObserver::UArcMassNavInvokerRemovalObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FMassNavInvokerFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcMassNavInvokerRemovalObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FMassNavInvokerFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcMassNavInvokerRemovalObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcMassNavInvokerSubsystem* Subsystem = Context.GetWorld()->GetSubsystem<UArcMassNavInvokerSubsystem>();
	if (Subsystem == nullptr)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNavInvokerRemove);

	ObserverQuery.ForEachEntityChunk(Context, [Subsystem](FMassExecutionContext& Ctx)
	{
		TConstArrayView<FMassNavInvokerFragment> Invokers = Ctx.GetFragmentView<FMassNavInvokerFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassNavInvokerFragment& Fragment = Invokers[EntityIt];
			if (Fragment.InvokerIndex != INDEX_NONE)
			{
				Subsystem->FreeSlot(Fragment.InvokerIndex);
			}
		}
	});
}
