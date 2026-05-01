// Copyright Lukasz Baran. All Rights Reserved.

#include "Strategy/ArcPopulationProcessor.h"
#include "Strategy/ArcPopulationFragment.h"
#include "Mass/ArcEconomyFragments.h"
#include "MassExecutionContext.h"

UArcPopulationProcessor::UArcPopulationProcessor()
    : SettlementQuery(*this)
{
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    ProcessingPhase = EMassProcessingPhase::PostPhysics;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcPopulationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    SettlementQuery.AddRequirement<FArcSettlementFragment>(EMassFragmentAccess::ReadOnly);
    SettlementQuery.AddRequirement<FArcSettlementMarketFragment>(EMassFragmentAccess::ReadOnly);
    SettlementQuery.AddRequirement<FArcSettlementWorkforceFragment>(EMassFragmentAccess::ReadOnly);
    SettlementQuery.AddRequirement<FArcPopulationFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcPopulationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    const float DeltaTime = Context.GetDeltaTimeSeconds();

    SettlementQuery.ForEachEntityChunk(EntityManager, Context,
        [DeltaTime](FMassExecutionContext& Context)
        {
            const int32 NumEntities = Context.GetNumEntities();
            const TConstArrayView<FArcSettlementMarketFragment> Markets = Context.GetFragmentView<FArcSettlementMarketFragment>();
            const TConstArrayView<FArcSettlementWorkforceFragment> Workforces = Context.GetFragmentView<FArcSettlementWorkforceFragment>();
            const TArrayView<FArcPopulationFragment> Populations = Context.GetMutableFragmentView<FArcPopulationFragment>();

            for (int32 Idx = 0; Idx < NumEntities; ++Idx)
            {
                FArcPopulationFragment& Pop = Populations[Idx];
                const FArcSettlementMarketFragment& Market = Markets[Idx];
                const FArcSettlementWorkforceFragment& Workforce = Workforces[Idx];

                // Sync population count from actual workforce
                const int32 ActualPopulation = Workforce.WorkerCount + Workforce.TransporterCount
                    + Workforce.GathererCount + Workforce.CaravanCount + Workforce.IdleCount;
                Pop.Population = ActualPopulation;

                // Check food status via storage
                const bool bHasFood = Market.CurrentTotalStorage > 0;

                // --- Starvation ---
                if (!bHasFood)
                {
                    Pop.StarvationTimer += DeltaTime;
                    Pop.GrowthTimer = 0.0f;
                }
                else
                {
                    Pop.StarvationTimer = FMath::Max(0.0f, Pop.StarvationTimer - DeltaTime);
                }

                // --- Growth ---
                const bool bHasCapacity = Pop.Population < Pop.MaxPopulation;
                const bool bIsStable = Pop.StarvationTimer <= 0.0f;

                if (bHasFood && bHasCapacity && bIsStable)
                {
                    Pop.GrowthTimer += DeltaTime;
                    if (Pop.GrowthTimer >= Pop.GrowthIntervalSeconds)
                    {
                        Pop.GrowthTimer -= Pop.GrowthIntervalSeconds;
                        // Growth signal — actual NPC spawning handled by settlement subsystem
                    }
                }
                else
                {
                    Pop.GrowthTimer = 0.0f;
                }
            }
        });
}
