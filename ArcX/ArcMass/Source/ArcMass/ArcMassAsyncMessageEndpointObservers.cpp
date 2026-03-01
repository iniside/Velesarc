// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassAsyncMessageEndpointObservers.h"
#include "ArcMassAsyncMessageEndpointFragment.h"
#include "AsyncMessageBindingEndpoint.h"
#include "MassExecutionContext.h"

// ====================================================================
// Add Observer
// ====================================================================

UArcMassAsyncMessageEndpointAddObserver::UArcMassAsyncMessageEndpointAddObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcMassAsyncMessageEndpointTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcMassAsyncMessageEndpointAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcMassAsyncMessageEndpointFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcMassAsyncMessageEndpointAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMassAsyncMessageEndpointFragment> EndpointList =
				Ctx.GetMutableFragmentView<FArcMassAsyncMessageEndpointFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMassAsyncMessageEndpointFragment& Fragment = EndpointList[EntityIt];
				if (!Fragment.Endpoint.IsValid())
				{
					Fragment.Endpoint = MakeShared<FAsyncMessageBindingEndpoint>();
				}
			}
		});
}

// ====================================================================
// Remove Observer
// ====================================================================

UArcMassAsyncMessageEndpointRemoveObserver::UArcMassAsyncMessageEndpointRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcMassAsyncMessageEndpointTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
}

void UArcMassAsyncMessageEndpointRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcMassAsyncMessageEndpointFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcMassAsyncMessageEndpointRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMassAsyncMessageEndpointFragment> EndpointList =
				Ctx.GetMutableFragmentView<FArcMassAsyncMessageEndpointFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcMassAsyncMessageEndpointFragment& Fragment = EndpointList[EntityIt];
				Fragment.Endpoint.Reset();
			}
		});
}
