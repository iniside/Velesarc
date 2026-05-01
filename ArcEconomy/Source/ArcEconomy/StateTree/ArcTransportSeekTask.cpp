// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcTransportSeekTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "ArcSettlementSubsystem.h"
#include "Items/ArcItemDefinition.h"
#include "MassEntityView.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcTransportSeekTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(NPCFragmentHandle);
    Linker.LinkExternalData(TransporterFragmentHandle);
    Linker.LinkExternalData(TransformHandle);
    return true;
}

EStateTreeRunStatus FArcTransportSeekTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    FArcTransportSeekTaskInstanceData& InstanceData = Context.GetInstanceData(*this);

    const FArcEconomyNPCFragment& NPC = Context.GetExternalData(NPCFragmentHandle);
    const FTransformFragment& Transform = Context.GetExternalData(TransformHandle);
    const FVector Location = Transform.GetTransform().GetLocation();

    UWorld* World = MassContext.GetWorld();
    UArcKnowledgeSubsystem* KnowledgeSub = World ? World->GetSubsystem<UArcKnowledgeSubsystem>() : nullptr;
    UArcSettlementSubsystem* SettlementSub = World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;
    if (!KnowledgeSub || !SettlementSub)
    {
        return EStateTreeRunStatus::Failed;
    }

    // Determine query radius based on caravan status
    FMassEntityManager& EntityManager = MassContext.GetEntityManager();
    const FMassEntityHandle EntityHandle = MassContext.GetEntity();
    const FMassEntityView EntityView(EntityManager, EntityHandle);
    const bool bIsCaravan = EntityView.HasTag<FArcCaravanTag>();

    float QueryRadius = 0.0f;
    if (bIsCaravan)
    {
        QueryRadius = 100000.0f; // Large radius for cross-settlement
    }
    else
    {
        const FArcSettlementFragment* Settlement = EntityManager.GetFragmentDataPtr<FArcSettlementFragment>(NPC.SettlementHandle);
        QueryRadius = Settlement ? Settlement->SettlementRadius : 5000.0f;
    }

    // Query for demand and supply entries (both queries done once, matched in memory)
    FGameplayTagQuery DemandTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Demand);
    TArray<FArcKnowledgeHandle> DemandHandles;
    KnowledgeSub->QueryKnowledgeInRadius(Location, QueryRadius, DemandHandles, DemandTagQuery);

    FGameplayTagQuery SupplyTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Supply);
    TArray<FArcKnowledgeHandle> SupplyHandles;
    KnowledgeSub->QueryKnowledgeInRadius(Location, QueryRadius, SupplyHandles, SupplyTagQuery);

    if (DemandHandles.Num() == 0 || SupplyHandles.Num() == 0)
    {
        return EStateTreeRunStatus::Failed;
    }

    // Match demand with supply in memory
    float BestScore = -TNumericLimits<float>::Max();
    FArcKnowledgeHandle BestDemandHandle;
    FArcKnowledgeHandle BestSupplyHandle;

    for (const FArcKnowledgeHandle& DemandHandle : DemandHandles)
    {
        const FArcKnowledgeEntry* DemandEntry = KnowledgeSub->GetKnowledgeEntry(DemandHandle);
        if (!DemandEntry)
        {
            continue;
        }

        const FArcEconomyDemandPayload* DemandPayload = DemandEntry->Payload.GetPtr<FArcEconomyDemandPayload>();
        if (!DemandPayload || !DemandPayload->ItemDefinition)
        {
            continue;
        }

        for (const FArcKnowledgeHandle& SupplyHandle : SupplyHandles)
        {
            const FArcKnowledgeEntry* SupplyEntry = KnowledgeSub->GetKnowledgeEntry(SupplyHandle);
            if (!SupplyEntry)
            {
                continue;
            }

            const FArcEconomySupplyPayload* SupplyPayload = SupplyEntry->Payload.GetPtr<FArcEconomySupplyPayload>();
            if (!SupplyPayload || SupplyPayload->ItemDefinition != DemandPayload->ItemDefinition)
            {
                continue;
            }

            float Score = 0.0f;

            if (bIsCaravan)
            {
                // Caravan: score by profit (sell price - buy price - travel cost)
                const float SellPrice = DemandPayload->OfferingPrice;
                const float BuyPrice = SupplyPayload->AskingPrice;
                const float Distance = FVector::Dist(SupplyEntry->Location, DemandEntry->Location);
                const float TravelCost = Distance * SettlementSub->TravelCostScale;
                Score = SellPrice - BuyPrice - TravelCost;
            }
            else
            {
                // Local: score by proximity (shorter = better)
                const float DistToSupply = FVector::Dist(Location, SupplyEntry->Location);
                const float DistToDemand = FVector::Dist(Location, DemandEntry->Location);
                Score = 10000.0f - (DistToSupply + DistToDemand);
            }

            if (Score > BestScore)
            {
                BestScore = Score;
                BestDemandHandle = DemandHandle;
                BestSupplyHandle = SupplyHandle;
            }
        }
    }

    if (!BestDemandHandle.IsValid() || !BestSupplyHandle.IsValid())
    {
        return EStateTreeRunStatus::Failed;
    }

    // Write results to instance data
    const FArcKnowledgeEntry* BestDemand = KnowledgeSub->GetKnowledgeEntry(BestDemandHandle);
    const FArcKnowledgeEntry* BestSupply = KnowledgeSub->GetKnowledgeEntry(BestSupplyHandle);
    const FArcEconomyDemandPayload* FinalDemandPayload = BestDemand->Payload.GetPtr<FArcEconomyDemandPayload>();
    const FArcEconomySupplyPayload* FinalSupplyPayload = BestSupply->Payload.GetPtr<FArcEconomySupplyPayload>();

    InstanceData.DemandBuildingHandle = BestDemand->SourceEntity;
    InstanceData.SupplyBuildingHandle = BestSupply->SourceEntity;
    InstanceData.ItemDefinition = FinalDemandPayload->ItemDefinition;
    InstanceData.Quantity = FMath::Min(FinalDemandPayload->QuantityNeeded, FinalSupplyPayload->QuantityAvailable);

    // Update transporter fragment
    FArcTransporterFragment& Transporter = Context.GetExternalData(TransporterFragmentHandle);
    Transporter.TaskState = EArcTransporterTaskState::PickingUp;
    Transporter.TargetBuildingHandle = InstanceData.SupplyBuildingHandle;
    Transporter.TargetItemDefinition = InstanceData.ItemDefinition;
    Transporter.TargetQuantity = InstanceData.Quantity;

    return EStateTreeRunStatus::Succeeded;
}
