// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcSettlementLinkObserver.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcSettlementSubsystem.h"
#include "ArcMass/PlacedEntities/ArcLinkableGuidSubsystem.h"
#include "ArcMass/PlacedEntities/ArcEntityRef.h"
#include "MassExecutionContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSettlementLinkObserver)

UArcSettlementLinkObserver::UArcSettlementLinkObserver()
    : ObserverQuery{*this}
{
    ObservedTypes.Add(FArcSettlementFragment::StaticStruct());
    ObservedOperations = EMassObservedOperationFlags::Add;
    bRequiresGameThreadExecution = true;
    ExecutionOrder.ExecuteAfter.Add(FName(TEXT("ArcLinkableGuidInit")));
}

void UArcSettlementLinkObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    ObserverQuery.AddRequirement<FArcSettlementFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcSettlementLinkObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    UWorld* World = EntityManager.GetWorld();
    UArcLinkableGuidSubsystem* GuidSubsystem = World ? World->GetSubsystem<UArcLinkableGuidSubsystem>() : nullptr;
    UArcSettlementSubsystem* SettlementSubsystem = World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;
    if (GuidSubsystem == nullptr || SettlementSubsystem == nullptr)
    {
        return;
    }

    ObserverQuery.ForEachEntityChunk(EntityManager, Context,
        [&EntityManager, GuidSubsystem, SettlementSubsystem](FMassExecutionContext& Ctx)
        {
            TConstArrayView<FArcSettlementFragment> Settlements =
                Ctx.GetFragmentView<FArcSettlementFragment>();

            for (int32 i = 0; i < Ctx.GetNumEntities(); ++i)
            {
                const FArcSettlementFragment& Settlement = Settlements[i];
                const FMassEntityHandle SettlementHandle = Ctx.GetEntity(i);

                for (const FArcEntityRef& Ref : Settlement.LinkedEntities)
                {
                    if (!Ref.IsValid())
                    {
                        continue;
                    }

                    const FMassEntityHandle LinkedHandle = GuidSubsystem->ResolveGuid(Ref.TargetGuid);
                    if (!LinkedHandle.IsValid())
                    {
                        continue;
                    }

                    FMassEntityView EntityView(EntityManager, LinkedHandle);
                    FArcBuildingFragment* BuildingFragment = EntityView.GetFragmentDataPtr<FArcBuildingFragment>();
                    if (BuildingFragment == nullptr)
                    {
                        continue;
                    }

                    if (BuildingFragment->SettlementHandle.IsValid())
                    {
                        continue;
                    }

                    BuildingFragment->SettlementHandle = SettlementHandle;
                    SettlementSubsystem->RegisterBuilding(SettlementHandle, LinkedHandle);
                }
            }
        });
}
