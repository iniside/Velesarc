// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcDemandGraph.h"
#include "Mass/ArcEconomyFragments.h"
#include "MassEntityManager.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemSpec.h"
#include "Mass/ArcMassItemFragments.h"
#include "Core/ArcCoreAssetManager.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "ArcEconomyUtils.h"

namespace ArcDemandGraph
{

void FDemandGraph::Reset()
{
    Nodes.Reset();
    Edges.Reset();
    PotentialProducerIndex.Reset();
    VisitedChainLinks.Reset();
}

int32 FDemandGraph::FindOrCreateActiveProducer(
    FMassEntityManager& EntityManager,
    UArcItemDefinition* ItemDef,
    const TArray<FMassEntityHandle>& BuildingHandles)
{
    // Check if we already have a producer node for this item.
    for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); ++NodeIdx)
    {
        const FNode& Node = Nodes[NodeIdx];
        if (Node.Type == ENodeType::Producer
            && Node.Status != ENodeStatus::Potential
            && Node.ItemDef == ItemDef)
        {
            return NodeIdx;
        }
    }

    // Scan buildings for one with DesiredRecipe outputting ItemDef.
    for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        const FArcBuildingFragment* BuildingFrag = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        const FArcBuildingWorkforceFragment* WorkforceFrag = EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(BuildingHandle);
        const FArcBuildingEconomyConfig* EconomyConfig = EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);

        if (!BuildingFrag || !WorkforceFrag || !EconomyConfig)
        {
            continue;
        }

        for (const FArcBuildingSlotData& Slot : WorkforceFrag->Slots)
        {
            if (!Slot.DesiredRecipe)
            {
                continue;
            }

            if (!Slot.DesiredRecipe->OutputItemDefinition.IsValid())
            {
                continue;
            }

            UArcItemDefinition* OutputItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(
                Slot.DesiredRecipe->OutputItemDefinition.AssetId);

            if (OutputItemDef != ItemDef)
            {
                continue;
            }

            const int32 ProducerIdx = Nodes.Num();

            FNode ProducerNode;
            ProducerNode.Type = ENodeType::Producer;
            ProducerNode.Entity = BuildingHandle;
            ProducerNode.BuildingName = BuildingFrag->BuildingName;
            ProducerNode.ItemDef = OutputItemDef;
            ProducerNode.StorageCap = EconomyConfig->OutputBufferSize;
            ProducerNode.CurrentStock = BuildingFrag->CurrentOutputCount;
            ProducerNode.bBackpressured = (BuildingFrag->CurrentOutputCount >= EconomyConfig->OutputBufferSize);
            ProducerNode.bStaffed = (Slot.RequiredWorkerCount > 0);

            if (ProducerNode.bBackpressured)
            {
                ProducerNode.Status = ENodeStatus::Backpressured;
            }
            else if (!ProducerNode.bStaffed)
            {
                ProducerNode.Status = ENodeStatus::Unstaffed;
            }
            else
            {
                ProducerNode.Status = ENodeStatus::Flowing;
            }

            Nodes.Add(ProducerNode);
            return ProducerIdx;
        }
    }

    // Check gathering buildings — they are active producers if they have gatherers assigned.
    for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        const FArcBuildingFragment* BuildingFrag = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        const FArcBuildingEconomyConfig* EconomyConfig = EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);

        if (!BuildingFrag || !EconomyConfig || !EconomyConfig->IsGatheringBuilding())
        {
            continue;
        }

        bool bProducesItem = false;
        for (UArcItemDefinition* GatherItem : EconomyConfig->GatherOutputItems)
        {
            if (GatherItem == ItemDef)
            {
                bProducesItem = true;
                break;
            }
        }
        if (!bProducesItem)
        {
            continue;
        }

        const int32 ProducerIdx = Nodes.Num();

        FNode ProducerNode;
        ProducerNode.Type = ENodeType::Producer;
        ProducerNode.Entity = BuildingHandle;
        ProducerNode.BuildingName = BuildingFrag->BuildingName;
        ProducerNode.ItemDef = ItemDef;
        ProducerNode.StorageCap = EconomyConfig->OutputBufferSize;
        ProducerNode.CurrentStock = BuildingFrag->CurrentOutputCount;
        ProducerNode.bBackpressured = (BuildingFrag->CurrentOutputCount >= EconomyConfig->OutputBufferSize);
        ProducerNode.bStaffed = true; // Staffing is determined by governor gatherer assignment

        if (ProducerNode.bBackpressured)
        {
            ProducerNode.Status = ENodeStatus::Backpressured;
        }
        else
        {
            ProducerNode.Status = ENodeStatus::Flowing;
        }

        Nodes.Add(ProducerNode);
        return ProducerIdx;
    }

    return INDEX_NONE;
}

int32 FDemandGraph::CreatePotentialProducerAndEnqueueInputs(
    FMassEntityManager& EntityManager,
    const FPotentialProducer& Producer,
    TArray<int32>& BFSQueue)
{
    const FArcBuildingFragment* BuildingFrag = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(Producer.BuildingHandle);
    const FArcBuildingEconomyConfig* EconomyConfig = EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(Producer.BuildingHandle);

    UArcItemDefinition* OutputItemDef = nullptr;
    if (Producer.Recipe && Producer.Recipe->OutputItemDefinition.IsValid())
    {
        OutputItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(Producer.Recipe->OutputItemDefinition.AssetId);
    }

    const int32 ProducerIdx = Nodes.Num();

    FNode ProducerNode;
    ProducerNode.Type = ENodeType::Producer;
    ProducerNode.Status = ENodeStatus::Potential;
    ProducerNode.Entity = Producer.BuildingHandle;
    ProducerNode.BuildingName = Producer.BuildingName;
    ProducerNode.ItemDef = OutputItemDef;
    if (EconomyConfig)
    {
        ProducerNode.StorageCap = EconomyConfig->OutputBufferSize;
    }
    if (BuildingFrag)
    {
        ProducerNode.CurrentStock = BuildingFrag->CurrentOutputCount;
    }

    Nodes.Add(ProducerNode);

    // Enqueue consumer nodes for each ingredient.
    if (!Producer.Recipe)
    {
        return ProducerIdx;
    }

    const int32 IngredientCount = Producer.Recipe->GetIngredientCount();
    for (int32 IngIdx = 0; IngIdx < IngredientCount; ++IngIdx)
    {
        const FArcRecipeIngredient* BaseIngredient = Producer.Recipe->GetIngredientBase(IngIdx);
        if (!BaseIngredient)
        {
            continue;
        }

        const FArcRecipeIngredient_ItemDef* ItemDefIngredient = Producer.Recipe->GetIngredient<FArcRecipeIngredient_ItemDef>(IngIdx);
        const FArcRecipeIngredient_Tags* TagsIngredient = Producer.Recipe->GetIngredient<FArcRecipeIngredient_Tags>(IngIdx);

        const int32 ConsumerIdx = Nodes.Num();

        FNode ConsumerNode;
        ConsumerNode.Type = ENodeType::Consumer;
        ConsumerNode.Entity = Producer.BuildingHandle;
        ConsumerNode.BuildingName = Producer.BuildingName;
        ConsumerNode.DemandQuantity = BaseIngredient->Amount;
        ConsumerNode.Status = ENodeStatus::UnmetDemand;

        if (ItemDefIngredient && ItemDefIngredient->ItemDefinitionId.IsValid())
        {
            UArcItemDefinition* IngItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(
                ItemDefIngredient->ItemDefinitionId.AssetId);
            ConsumerNode.ItemDef = IngItemDef;
        }
        else if (TagsIngredient)
        {
            ConsumerNode.ItemTag = TagsIngredient->RequiredTags.IsEmpty()
                ? FGameplayTag()
                : TagsIngredient->RequiredTags.First();
        }
        else
        {
            // Unknown ingredient type; skip.
            continue;
        }

        Nodes.Add(ConsumerNode);
        BFSQueue.Add(ConsumerIdx);
    }

    return ProducerIdx;
}

void FDemandGraph::Rebuild(
    FMassEntityManager& EntityManager,
    FMassEntityHandle SettlementEntity,
    const TArray<FMassEntityHandle>& BuildingHandles,
    const FArcSettlementMarketFragment& Market)
{
    Reset();

    // Node 0 is always the settlement node.
    {
        FNode SettlementNode;
        SettlementNode.Type = ENodeType::Settlement;
        SettlementNode.Entity = SettlementEntity;
        SettlementNode.CurrentStock = Market.CurrentTotalStorage;
        SettlementNode.StorageCap = Market.TotalStorageCap;
        SettlementNode.bBackpressured = (Market.CurrentTotalStorage >= Market.TotalStorageCap);
        SettlementNode.Status = SettlementNode.bBackpressured ? ENodeStatus::Backpressured : ENodeStatus::Flowing;
        Nodes.Add(SettlementNode);
    }

    // -------------------------------------------------------------------------
    // Pass 1: Build PotentialProducerIndex
    // For every building, scan AllowedRecipes (via ResolveAllowedRecipes) and
    // map output item def -> buildings that could produce it.
    // -------------------------------------------------------------------------
    for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        const FArcBuildingFragment* BuildingFrag = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        FArcBuildingEconomyConfig* EconomyConfig =
            EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);

        if (!BuildingFrag || !EconomyConfig)
        {
            continue;
        }

        const TArray<UArcRecipeDefinition*>& AllowedRecipes = ArcEconomy::ResolveAllowedRecipes(*EconomyConfig);
        for (UArcRecipeDefinition* Recipe : AllowedRecipes)
        {
            if (!Recipe)
            {
                continue;
            }

            if (!Recipe->OutputItemDefinition.IsValid())
            {
                continue;
            }

            UArcItemDefinition* OutputItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(
                Recipe->OutputItemDefinition.AssetId);
            if (!OutputItemDef)
            {
                continue;
            }

            FPotentialProducer PotentialProducer;
            PotentialProducer.BuildingHandle = BuildingHandle;
            PotentialProducer.Recipe = Recipe;
            PotentialProducer.BuildingName = BuildingFrag->BuildingName;

            TArray<FPotentialProducer>& ProducersForItem = PotentialProducerIndex.FindOrAdd(
                TObjectPtr<UArcItemDefinition>(OutputItemDef));
            ProducersForItem.Add(PotentialProducer);
        }

        // Gathering buildings: register as potential producers for each GatherOutputItem.
        // These are leaf producers (no recipe, no ingredients).
        if (EconomyConfig->IsGatheringBuilding())
        {
            for (UArcItemDefinition* GatherItem : EconomyConfig->GatherOutputItems)
            {
                if (!GatherItem)
                {
                    continue;
                }

                FPotentialProducer PotentialProducer;
                PotentialProducer.BuildingHandle = BuildingHandle;
                PotentialProducer.Recipe = nullptr; // No recipe — gathering
                PotentialProducer.BuildingName = BuildingFrag->BuildingName;

                TArray<FPotentialProducer>& ProducersForItem = PotentialProducerIndex.FindOrAdd(
                    TObjectPtr<UArcItemDefinition>(GatherItem));
                ProducersForItem.Add(PotentialProducer);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Pass 2: Seed consumer nodes from ConsumptionNeeds
    // Buildings with ConsumptionNeeds that are below desired stock level.
    // Seeds the BFS queue.
    // -------------------------------------------------------------------------
    TArray<int32> BFSQueue;

    for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        const FArcBuildingFragment* BuildingFrag = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        const FArcBuildingEconomyConfig* EconomyConfig = EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);

        if (!BuildingFrag || !EconomyConfig)
        {
            continue;
        }

        if (EconomyConfig->ConsumptionNeeds.IsEmpty())
        {
            continue;
        }

        const FArcMassItemSpecArrayFragment* ItemStorage = EntityManager.GetFragmentDataPtr<FArcMassItemSpecArrayFragment>(BuildingHandle);

        for (const FArcBuildingConsumptionEntry& Need : EconomyConfig->ConsumptionNeeds)
        {
            UArcItemDefinition* NeedItemDef = Need.Item;

            int32 CurrentStock = 0;
            if (ItemStorage)
            {
                for (const FArcItemSpec& ItemSpec : ItemStorage->Items)
                {
                    if (NeedItemDef)
                    {
                        const UArcItemDefinition* SpecItemDef = ItemSpec.GetItemDefinition();
                        if (SpecItemDef == NeedItemDef)
                        {
                            CurrentStock += ItemSpec.Amount;
                        }
                    }
                    else if (Need.ItemTag.IsValid())
                    {
                        const UArcItemDefinition* SpecItemDef = ItemSpec.GetItemDefinition();
                        if (SpecItemDef)
                        {
                            const FArcItemFragment_Tags* TagsFrag = SpecItemDef->FindFragment<FArcItemFragment_Tags>();
                            if (TagsFrag && TagsFrag->AssetTags.HasTag(Need.ItemTag))
                            {
                                CurrentStock += ItemSpec.Amount;
                            }
                        }
                    }
                }
            }

            const int32 Deficit = Need.DesiredStockLevel - CurrentStock;
            if (Deficit <= 0)
            {
                continue;
            }

            const int32 ConsumerIdx = Nodes.Num();

            FNode ConsumerNode;
            ConsumerNode.Type = ENodeType::Consumer;
            ConsumerNode.Entity = BuildingHandle;
            ConsumerNode.BuildingName = BuildingFrag->BuildingName;
            ConsumerNode.ItemDef = NeedItemDef;
            ConsumerNode.ItemTag = Need.ItemTag;
            ConsumerNode.DemandQuantity = Deficit;
            ConsumerNode.CurrentStock = CurrentStock;
            ConsumerNode.StorageCap = Need.DesiredStockLevel;
            ConsumerNode.Status = ENodeStatus::UnmetDemand;

            Nodes.Add(ConsumerNode);
            BFSQueue.Add(ConsumerIdx);
        }
    }

    // Also seed consumer nodes from production input needs (halted slots with active demand handles).
    for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        const FArcBuildingFragment* BuildingFrag = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        const FArcBuildingWorkforceFragment* WorkforceFrag = EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(BuildingHandle);

        if (!BuildingFrag || !WorkforceFrag)
        {
            continue;
        }

        for (const FArcBuildingSlotData& Slot : WorkforceFrag->Slots)
        {
            if (!Slot.DesiredRecipe || Slot.HaltDuration <= 0.0f)
            {
                continue;
            }

            const int32 IngredientCount = Slot.DesiredRecipe->GetIngredientCount();

            for (int32 IngIdx = 0; IngIdx < IngredientCount; ++IngIdx)
            {
                if (!Slot.DemandKnowledgeHandles.IsValidIndex(IngIdx))
                {
                    continue;
                }
                if (!Slot.DemandKnowledgeHandles[IngIdx].IsValid())
                {
                    continue;
                }

                const FArcRecipeIngredient* BaseIngredient = Slot.DesiredRecipe->GetIngredientBase(IngIdx);
                if (!BaseIngredient)
                {
                    continue;
                }

                const FArcRecipeIngredient_ItemDef* ItemDefIngredient =
                    Slot.DesiredRecipe->GetIngredient<FArcRecipeIngredient_ItemDef>(IngIdx);
                const FArcRecipeIngredient_Tags* TagsIngredient =
                    Slot.DesiredRecipe->GetIngredient<FArcRecipeIngredient_Tags>(IngIdx);

                const int32 ConsumerIdx = Nodes.Num();

                FNode ConsumerNode;
                ConsumerNode.Type = ENodeType::Consumer;
                ConsumerNode.Entity = BuildingHandle;
                ConsumerNode.BuildingName = BuildingFrag->BuildingName;
                ConsumerNode.DemandQuantity = BaseIngredient->Amount;
                ConsumerNode.Status = ENodeStatus::UnmetDemand;

                if (ItemDefIngredient && ItemDefIngredient->ItemDefinitionId.IsValid())
                {
                    UArcItemDefinition* IngItemDef = UArcCoreAssetManager::GetAsset<UArcItemDefinition>(
                        ItemDefIngredient->ItemDefinitionId.AssetId);
                    ConsumerNode.ItemDef = IngItemDef;
                }
                else if (TagsIngredient)
                {
                    ConsumerNode.ItemTag = TagsIngredient->RequiredTags.IsEmpty()
                        ? FGameplayTag()
                        : TagsIngredient->RequiredTags.First();
                }
                else
                {
                    continue;
                }

                Nodes.Add(ConsumerNode);
                BFSQueue.Add(ConsumerIdx);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Pass 3: Demand-pull BFS
    // For each consumer node in the queue, try to find or create a producer.
    // If a producer is found, link with an edge and set consumer status to Flowing.
    // If the producer is a potential one, enqueue its ingredient consumers too.
    // -------------------------------------------------------------------------
    for (int32 QueuePos = 0; QueuePos < BFSQueue.Num(); ++QueuePos)
    {
        const int32 ConsumerIdx = BFSQueue[QueuePos];
        // Note: Nodes array may grow during BFS; fetch by index each iteration.
        FNode& ConsumerNode = Nodes[ConsumerIdx];

        UArcItemDefinition* NeedItemDef = ConsumerNode.ItemDef;
        const FGameplayTag NeedItemTag = ConsumerNode.ItemTag;

        int32 ProducerIdx = INDEX_NONE;

        if (NeedItemDef)
        {
            // 1. Try active producer first (building with DesiredRecipe).
            ProducerIdx = FindOrCreateActiveProducer(EntityManager, NeedItemDef, BuildingHandles);

            // 2. If no active producer, look in PotentialProducerIndex.
            if (ProducerIdx == INDEX_NONE)
            {
                TArray<FPotentialProducer>* PotentialList = PotentialProducerIndex.Find(
                    TObjectPtr<UArcItemDefinition>(NeedItemDef));
                if (PotentialList)
                {
                    for (const FPotentialProducer& Potential : *PotentialList)
                    {
                        FChainLink ChainKey;
                        ChainKey.BuildingHandle = Potential.BuildingHandle;
                        ChainKey.Recipe = Potential.Recipe;

                        if (VisitedChainLinks.Contains(ChainKey))
                        {
                            continue;
                        }

                        VisitedChainLinks.Add(ChainKey);
                        ProducerIdx = CreatePotentialProducerAndEnqueueInputs(EntityManager, Potential, BFSQueue);
                        // Use first viable potential producer.
                        break;
                    }
                }
            }
        }
        else if (NeedItemTag.IsValid())
        {
            // Tag-based: try active producers first.
            for (const TPair<TObjectPtr<UArcItemDefinition>, TArray<FPotentialProducer>>& PotentialEntry : PotentialProducerIndex)
            {
                const UArcItemDefinition* KeyItemDef = PotentialEntry.Key;
                if (!KeyItemDef)
                {
                    continue;
                }
                const FArcItemFragment_Tags* TagsFrag = KeyItemDef->FindFragment<FArcItemFragment_Tags>();
                if (!TagsFrag || !TagsFrag->AssetTags.HasTag(NeedItemTag))
                {
                    continue;
                }

                // Try active producer for this item def.
                ProducerIdx = FindOrCreateActiveProducer(
                    EntityManager,
                    const_cast<UArcItemDefinition*>(KeyItemDef),
                    BuildingHandles);

                if (ProducerIdx != INDEX_NONE)
                {
                    break;
                }

                // No active producer; use first potential one.
                for (const FPotentialProducer& Potential : PotentialEntry.Value)
                {
                    FChainLink ChainKey;
                    ChainKey.BuildingHandle = Potential.BuildingHandle;
                    ChainKey.Recipe = Potential.Recipe;

                    if (VisitedChainLinks.Contains(ChainKey))
                    {
                        continue;
                    }

                    VisitedChainLinks.Add(ChainKey);
                    ProducerIdx = CreatePotentialProducerAndEnqueueInputs(EntityManager, Potential, BFSQueue);
                    break;
                }

                if (ProducerIdx != INDEX_NONE)
                {
                    break;
                }
            }
        }

        // Link consumer to producer if found.
        if (ProducerIdx != INDEX_NONE)
        {
            FEdge Edge;
            Edge.ConsumerIdx = ConsumerIdx;
            Edge.ProducerIdx = ProducerIdx;
            Edge.DemandQuantity = Nodes[ConsumerIdx].DemandQuantity;
            Edges.Add(Edge);
            Nodes[ConsumerIdx].Status = ENodeStatus::Flowing;
        }
        // else: consumer stays UnmetDemand
    }
}

void FDemandGraph::FindUnmetDemand(TArray<FUnmetDemandEntry>& OutUnmet) const
{
    OutUnmet.Reset();

    // Group consumer nodes with UnmetDemand status by item def (or item tag for tag-based).
    for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); ++NodeIdx)
    {
        const FNode& Node = Nodes[NodeIdx];
        if (Node.Type != ENodeType::Consumer || Node.Status != ENodeStatus::UnmetDemand)
        {
            continue;
        }

        // Find or create entry keyed by ItemDef (primary) or ItemTag (fallback).
        bool bFound = false;
        for (FUnmetDemandEntry& Entry : OutUnmet)
        {
            const bool bSameItemDef = (Node.ItemDef != nullptr && Entry.ItemDef == Node.ItemDef);
            const bool bSameTag = (Node.ItemDef == nullptr && Node.ItemTag.IsValid() && Entry.ItemTag == Node.ItemTag);
            if (bSameItemDef || bSameTag)
            {
                Entry.TotalDemand += Node.DemandQuantity;
                Entry.ConsumerNodeIndices.Add(NodeIdx);
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            FUnmetDemandEntry NewEntry;
            NewEntry.ItemDef = Node.ItemDef;
            NewEntry.ItemTag = Node.ItemTag;
            NewEntry.TotalDemand = Node.DemandQuantity;
            NewEntry.ConsumerNodeIndices.Add(NodeIdx);
            OutUnmet.Add(NewEntry);
        }
    }
}

void FDemandGraph::FindBottlenecks(TArray<FBottleneckEntry>& OutBottlenecks) const
{
    OutBottlenecks.Reset();

    for (int32 NodeIdx = 0; NodeIdx < Nodes.Num(); ++NodeIdx)
    {
        const FNode& Node = Nodes[NodeIdx];
        if (Node.Type != ENodeType::Producer)
        {
            continue;
        }

        if (Node.Status == ENodeStatus::Backpressured || Node.Status == ENodeStatus::Unstaffed)
        {
            FBottleneckEntry Entry;
            Entry.ProducerNodeIdx = NodeIdx;
            Entry.Reason = Node.Status;
            OutBottlenecks.Add(Entry);
        }
    }
}

} // namespace ArcDemandGraph
