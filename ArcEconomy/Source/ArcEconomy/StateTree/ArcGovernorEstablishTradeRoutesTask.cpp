// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcGovernorEstablishTradeRoutesTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "ArcEconomyTypes.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "Items/ArcItemDefinition.h"
#include "MassEntityView.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcGovernorEstablishTradeRoutesTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(SettlementHandle);
    Linker.LinkExternalData(MarketHandle);
    Linker.LinkExternalData(WorkforceHandle);
    Linker.LinkExternalData(TransformHandle);
    return true;
}

EStateTreeRunStatus FArcGovernorEstablishTradeRoutesTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    checkf(IsInGameThread(), TEXT("Governor tasks must run on game thread for cross-entity access"));
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    FMassEntityManager& EntityManager = MassContext.GetEntityManager();
    const FMassEntityHandle SettlementEntity = MassContext.GetEntity();
    const FArcGovernorEstablishTradeRoutesInstanceData& InstanceData = Context.GetInstanceData<FArcGovernorEstablishTradeRoutesInstanceData>(*this);

    UWorld* World = Context.GetWorld();
    UArcSettlementSubsystem* SettlementSub = World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;
    UArcKnowledgeSubsystem* KnowledgeSub = World ? World->GetSubsystem<UArcKnowledgeSubsystem>() : nullptr;
    if (!SettlementSub || !KnowledgeSub)
    {
        return EStateTreeRunStatus::Failed;
    }

    const FArcSettlementFragment& Settlement = Context.GetExternalData(SettlementHandle);
    const FArcSettlementMarketFragment& Market = Context.GetExternalData(MarketHandle);
    const FArcSettlementWorkforceFragment& Workforce = Context.GetExternalData(WorkforceHandle);

    // Only allocate caravans if we have idle NPCs
    if (Workforce.TransporterCount == 0 && Workforce.IdleCount == 0)
    {
        return EStateTreeRunStatus::Succeeded;
    }

    // Query neighbor settlement markets via knowledge
    FGameplayTagQuery MarketTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Market);
    TArray<FArcKnowledgeHandle> MarketHandles;
    const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
    KnowledgeSub->QueryKnowledgeInRadius(Transform.GetTransform().GetLocation(), 100000.0f, MarketHandles, MarketTagQuery);

    // Find best trade opportunity
    float BestDifferential = 0.0f;

    for (const FArcKnowledgeHandle& MarketEntryHandle : MarketHandles)
    {
        const FArcKnowledgeEntry* MarketEntry = KnowledgeSub->GetKnowledgeEntry(MarketEntryHandle);
        if (!MarketEntry || MarketEntry->SourceEntity == SettlementEntity)
        {
            continue; // Skip own market
        }

        const FArcEconomyMarketPayload* NeighborMarket = MarketEntry->Payload.GetPtr<FArcEconomyMarketPayload>();
        if (!NeighborMarket)
        {
            continue;
        }

        // Compare prices -- find items where our price differs significantly
        for (const TPair<TObjectPtr<UArcItemDefinition>, float>& NeighborPrice : NeighborMarket->PriceSnapshot)
        {
            const FArcResourceMarketData* OurData = Market.PriceTable.Find(NeighborPrice.Key);
            if (!OurData)
            {
                continue;
            }

            const float OurPrice = OurData->Price;
            const float TheirPrice = NeighborPrice.Value;

            float Differential = 0.0f;
            if (OurPrice > 0.0f && TheirPrice > 0.0f)
            {
                Differential = FMath::Max(OurPrice / TheirPrice, TheirPrice / OurPrice);
            }

            if (Differential > BestDifferential)
            {
                BestDifferential = Differential;
            }
        }
    }

    // If trade opportunity exists and exceeds threshold, assign an idle caravan NPC as transporter
    if (BestDifferential >= InstanceData.TradePriceDifferentialThreshold && Workforce.IdleCount > 0)
    {
        const TArray<FMassEntityHandle>& NPCHandles = SettlementSub->GetNPCsForSettlement(SettlementEntity);
        for (const FMassEntityHandle& NPCHandle : NPCHandles)
        {
            if (!EntityManager.IsEntityValid(NPCHandle))
            {
                continue;
            }

            FArcEconomyNPCFragment* NPC = EntityManager.GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
            if (NPC && NPC->Role == EArcEconomyNPCRole::Idle)
            {
                const FMassEntityView NPCView(EntityManager, NPCHandle);
                const bool bIsCaravan = NPCView.HasTag<FArcCaravanTag>();

                if (bIsCaravan)
                {
                    NPC->Role = EArcEconomyNPCRole::Transporter;
                    FArcTransporterFragment* Transporter = EntityManager.GetFragmentDataPtr<FArcTransporterFragment>(NPCHandle);
                    if (Transporter)
                    {
                        Transporter->TaskState = EArcTransporterTaskState::Idle;
                    }
                    break;
                }
            }
        }
    }

    return EStateTreeRunStatus::Succeeded;
}
