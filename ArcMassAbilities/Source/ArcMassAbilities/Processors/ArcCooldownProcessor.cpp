// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcCooldownProcessor.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "MassExecutionContext.h"

UArcCooldownProcessor::UArcCooldownProcessor()
    : EntityQuery{*this}
{
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = false;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Standalone | EProcessorExecutionFlags::Server);
    ExecutionOrder.ExecuteInGroup = FName(TEXT("Tasks"));
}

void UArcCooldownProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    EntityQuery.AddRequirement<FArcAbilityCooldownFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcCooldownProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = Context.GetDeltaTimeSeconds();

    EntityQuery.ForEachEntityChunk(Context,
        [DeltaTime](FMassExecutionContext& ChunkContext)
    {
        const int32 NumEntities = ChunkContext.GetNumEntities();
        TArrayView<FArcAbilityCooldownFragment> Cooldowns = ChunkContext.GetMutableFragmentView<FArcAbilityCooldownFragment>();

        for (int32 Idx = 0; Idx < NumEntities; ++Idx)
        {
            FArcAbilityCooldownFragment& Frag = Cooldowns[Idx];

            for (int32 CdIdx = Frag.ActiveCooldowns.Num() - 1; CdIdx >= 0; --CdIdx)
            {
                FArcCooldownEntry& Entry = Frag.ActiveCooldowns[CdIdx];
                Entry.RemainingTime -= DeltaTime;
                if (Entry.RemainingTime <= 0.f)
                {
                    Frag.ActiveCooldowns.RemoveAtSwap(CdIdx);
                }
            }
        }
    });
}
