// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcSettlementMarketProcessor.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/EntityFragments.h"
#include "Mass/ArcSettlementNeedUtils.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Items/ArcItemDefinition.h"
#include "ArcItemEconomyFragment.h"
#include "MassExecutionContext.h"

UArcSettlementMarketProcessor::UArcSettlementMarketProcessor()
    : SettlementQuery{*this}
{
    bAutoRegisterWithProcessingPhases = true;
    bRequiresGameThreadExecution = true;
    ProcessingPhase = EMassProcessingPhase::DuringPhysics;
    ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
}

void UArcSettlementMarketProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
    SettlementQuery.AddRequirement<FArcSettlementFragment>(EMassFragmentAccess::ReadOnly);
    SettlementQuery.AddRequirement<FArcSettlementMarketFragment>(EMassFragmentAccess::ReadWrite);
    SettlementQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcSettlementMarketProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
    TimeSinceLastTick += Context.GetDeltaTimeSeconds();
    if (TimeSinceLastTick < TickInterval)
    {
        return;
    }
    TimeSinceLastTick = 0.0f;

    UArcKnowledgeSubsystem* KnowledgeSub = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
    if (!KnowledgeSub)
    {
        return;
    }

    TRACE_CPUPROFILER_EVENT_SCOPE(ArcSettlementMarket);

    SettlementQuery.ForEachEntityChunk(Context, [KnowledgeSub, &EntityManager](FMassExecutionContext& Ctx)
    {
        const TConstArrayView<FArcSettlementFragment> Settlements = Ctx.GetFragmentView<FArcSettlementFragment>();
        const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
        TArrayView<FArcSettlementMarketFragment> Markets = Ctx.GetMutableFragmentView<FArcSettlementMarketFragment>();
        const int32 NumEntities = Ctx.GetNumEntities();

        for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
        {
            const FArcSettlementFragment& Settlement = Settlements[EntityIndex];
            FArcSettlementMarketFragment& Market = Markets[EntityIndex];
            const FMassEntityHandle Entity = Ctx.GetEntity(EntityIndex);
            const float K = Market.K;

            // Adjust prices based on supply/demand counters
            for (TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& Pair : Market.PriceTable)
            {
                FArcResourceMarketData& Data = Pair.Value;
                const float Supply = Data.SupplyCounter;
                const float Demand = Data.DemandCounter;
                const float MaxSD = FMath::Max3(Supply, Demand, 1.0f);

                Data.Price = Data.Price * (1.0f + K * (Demand - Supply) / MaxSD);

                // Clamp to item min/max from item economy fragment
                const UArcItemDefinition* ItemDef = Pair.Key;
                if (ItemDef)
                {
                    const FArcItemEconomyFragment* EconFragment = ItemDef->FindFragment<FArcItemEconomyFragment>();
                    if (EconFragment)
                    {
                        Data.Price = FMath::Clamp(Data.Price, EconFragment->MinPrice, EconFragment->MaxPrice);
                    }
                }

                // Reset counters
                Data.SupplyCounter = 0.0f;
                Data.DemandCounter = 0.0f;
            }

            // Compute total storage from all supply sources
            int32 TotalStorage = 0;
            for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& StoragePair : Market.PriceTable)
            {
                for (const FArcMarketSupplySource& Source : StoragePair.Value.SupplySources)
                {
                    TotalStorage += Source.Quantity;
                }
            }
            Market.CurrentTotalStorage = TotalStorage;

            // Update settlement needs from current supply levels
            ArcEconomy::SettlementNeeds::UpdateNeedsFromMarket(EntityManager, Entity, Market);

            // Post/update SettlementMarket knowledge entry
            KnowledgeSub->RemoveKnowledgeBySource(Entity);

            FArcKnowledgeEntry MarketEntry;
            MarketEntry.Tags.AddTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Market);
            MarketEntry.Location = Transforms[EntityIndex].GetTransform().GetLocation();
            MarketEntry.SourceEntity = Entity;
            MarketEntry.Relevance = 1.0f;

            FArcEconomyMarketPayload MarketPayload;
            for (const TPair<TObjectPtr<UArcItemDefinition>, FArcResourceMarketData>& Pair : Market.PriceTable)
            {
                MarketPayload.PriceSnapshot.Add(Pair.Key, Pair.Value.Price);
            }
            MarketEntry.Payload.InitializeAs<FArcEconomyMarketPayload>(MarketPayload);

            KnowledgeSub->RegisterKnowledge(MarketEntry);
        }
    });
}
