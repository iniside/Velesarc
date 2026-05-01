// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcConditionTickProcessors.h"

#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

UArcConditionTickProcessor::UArcConditionTickProcessor()
    : EntityQuery{*this}
{
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
    ProcessingPhase = EMassProcessingPhase::DuringPhysics;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcConditionTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddRequirement<FArcConditionStatesFragment>(EMassFragmentAccess::ReadWrite);
    EntityQuery.AddConstSharedRequirement<FArcConditionConfigsShared>(EMassFragmentPresence::All);
}

void UArcConditionTickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = Context.GetDeltaTimeSeconds();
    if (DeltaTime <= 0.f) { return; }

    TRACE_CPUPROFILER_EVENT_SCOPE(ArcConditionTick);

    TArray<FMassEntityHandle> StateChangedEntities;
    TArray<FMassEntityHandle> OverloadChangedEntities;

    EntityQuery.ForEachEntityChunk(Context,
        [DeltaTime, &StateChangedEntities, &OverloadChangedEntities](FMassExecutionContext& Ctx)
        {
            TArrayView<FArcConditionStatesFragment> Fragments = Ctx.GetMutableFragmentView<FArcConditionStatesFragment>();
            const FArcConditionConfigsShared& SharedConfig = Ctx.GetConstSharedFragment<FArcConditionConfigsShared>();

            for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
            {
                FArcConditionStatesFragment& Fragment = Fragments[EntityIt];
                bool bAnyStateChanged = false;
                bool bAnyOverloadChanged = false;

                for (int32 i = 0; i < ArcConditionTypeCount; ++i)
                {
                    bool bStateChanged = false;
                    bool bOverloadChanged = false;
                    ArcConditionHelpers::TickCondition(Fragment.States[i], SharedConfig.Configs[i], DeltaTime, bStateChanged, bOverloadChanged);

                    bAnyStateChanged |= bStateChanged;
                    bAnyOverloadChanged |= bOverloadChanged;
                }

                if (bAnyStateChanged)    { StateChangedEntities.Add(Ctx.GetEntity(EntityIt)); }
                if (bAnyOverloadChanged) { OverloadChangedEntities.Add(Ctx.GetEntity(EntityIt)); }
            }
        }
    );

    UWorld* World = EntityManager.GetWorld();
    UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
    if (SignalSubsystem)
    {
        if (StateChangedEntities.Num() > 0)
        {
            SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionStateChanged, StateChangedEntities);
        }
        if (OverloadChangedEntities.Num() > 0)
        {
            SignalSubsystem->SignalEntities(UE::ArcConditionEffects::Signals::ConditionOverloadChanged, OverloadChangedEntities);
        }
    }
}
