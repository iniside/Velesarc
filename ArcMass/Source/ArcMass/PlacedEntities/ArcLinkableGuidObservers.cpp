// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/PlacedEntities/ArcLinkableGuidObservers.h"
#include "ArcMass/PlacedEntities/ArcLinkableGuid.h"
#include "ArcMass/PlacedEntities/ArcLinkableGuidSubsystem.h"
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcLinkableGuidObservers)

// -----------------------------------------------------------------------------
// Init Observer
// -----------------------------------------------------------------------------

UArcLinkableGuidInitObserver::UArcLinkableGuidInitObserver()
    : ObserverQuery{*this}
{
    ObservedTypes.Add(FArcLinkableGuidFragment::StaticStruct());
    ObservedOperations = EMassObservedOperationFlags::Add;
    bRequiresGameThreadExecution = true;
    ExecutionOrder.ExecuteInGroup = FName(TEXT("ArcLinkableGuidInit"));
}

void UArcLinkableGuidInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    ObserverQuery.AddRequirement<FArcLinkableGuidFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcLinkableGuidInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    UArcLinkableGuidSubsystem* Subsystem = World ? World->GetSubsystem<UArcLinkableGuidSubsystem>() : nullptr;
    if (Subsystem == nullptr)
    {
        return;
    }

    ObserverQuery.ForEachEntityChunk(EntityManager, Context,
        [Subsystem](FMassExecutionContext& Ctx)
        {
            TConstArrayView<FArcLinkableGuidFragment> GuidFragments =
                Ctx.GetFragmentView<FArcLinkableGuidFragment>();

            for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
            {
                const FArcLinkableGuidFragment& GuidFrag = GuidFragments[i];
                if (GuidFrag.LinkGuid.IsValid())
                {
                    Subsystem->RegisterEntity(GuidFrag.LinkGuid, Ctx.GetEntity(i));
                }
            }
        });
}

// -----------------------------------------------------------------------------
// Deinit Observer
// -----------------------------------------------------------------------------

UArcLinkableGuidDeinitObserver::UArcLinkableGuidDeinitObserver()
    : ObserverQuery{*this}
{
    ObservedTypes.Add(FArcLinkableGuidFragment::StaticStruct());
    ObservedOperations = EMassObservedOperationFlags::Remove;
    bRequiresGameThreadExecution = true;
}

void UArcLinkableGuidDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    ObserverQuery.AddRequirement<FArcLinkableGuidFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcLinkableGuidDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    UArcLinkableGuidSubsystem* Subsystem = World ? World->GetSubsystem<UArcLinkableGuidSubsystem>() : nullptr;
    if (Subsystem == nullptr)
    {
        return;
    }

    ObserverQuery.ForEachEntityChunk(EntityManager, Context,
        [Subsystem](FMassExecutionContext& Ctx)
        {
            TConstArrayView<FArcLinkableGuidFragment> GuidFragments =
                Ctx.GetFragmentView<FArcLinkableGuidFragment>();

            for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
            {
                const FArcLinkableGuidFragment& GuidFrag = GuidFragments[i];
                if (GuidFrag.LinkGuid.IsValid())
                {
                    Subsystem->UnregisterEntity(GuidFrag.LinkGuid);
                }
            }
        });
}
