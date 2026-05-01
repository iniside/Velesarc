// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcGovernorLogic.h"
#include "MassEntityManager.h"
#include "Mass/ArcEconomyFragments.h"
#include "Data/ArcGovernorDataAsset.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "Items/ArcItemDefinition.h"
#include "ArcEconomyUtils.h"
#include "Knowledge/ArcEconomyKnowledgeTypes.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeEntry.h"
#include "MassEntityView.h"
#include "Mass/EntityFragments.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "Items/ArcItemSpec.h"
#include "Core/ArcCoreAssetManager.h"
#include "SmartObjectDefinition.h"
#include "ArcAreaSubsystem.h"
#include "Mass/ArcAreaFragments.h"
#include "ArcAreaTypes.h"
#include "ArcMass/SmartObject/ArcMassSmartObjectFragments.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "Data/ArcSettlementNeedTypes.h"
#include "Data/ArcSettlementNeedDataAsset.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"

namespace ArcGovernor
{

FArcBuildingSlotSummary ClassifyBuildingSlots(
    const USmartObjectDefinition& SODefinition,
    const TMap<EArcEconomyNPCRole, FGameplayTagContainer>& SlotRoleMapping)
{
    FArcBuildingSlotSummary Summary;

    const TConstArrayView<FSmartObjectSlotDefinition> SlotDefs = SODefinition.GetSlots();
    for (int32 SlotIdx = 0; SlotIdx < SlotDefs.Num(); ++SlotIdx)
    {
        FGameplayTagContainer SlotActivityTags;
        SODefinition.GetSlotActivityTags(SlotIdx, SlotActivityTags);

        for (const TPair<EArcEconomyNPCRole, FGameplayTagContainer>& Mapping : SlotRoleMapping)
        {
            if (Mapping.Key == EArcEconomyNPCRole::Idle)
            {
                continue;
            }

            if (SlotActivityTags.HasAll(Mapping.Value))
            {
                if (Mapping.Key == EArcEconomyNPCRole::Worker)
                {
                    ++Summary.TotalWorkerSlots;
                }
                else if (Mapping.Key == EArcEconomyNPCRole::Transporter)
                {
                    ++Summary.TotalTransportSlots;
                }
                else if (Mapping.Key == EArcEconomyNPCRole::Gatherer)
                {
                    ++Summary.TotalGathererSlots;
                }
                break;
            }
        }
    }

    Summary.FreeWorkerSlots = Summary.TotalWorkerSlots;
    Summary.FreeTransportSlots = Summary.TotalTransportSlots;
    Summary.FreeGathererSlots = Summary.TotalGathererSlots;
    return Summary;
}

void PreFetchNPCs(FMassEntityManager& EntityManager, const TArray<FMassEntityHandle>& NPCHandles, TArray<FNPCData>& OutNPCs)
{
    OutNPCs.Reserve(NPCHandles.Num());
    for (const FMassEntityHandle& NPCHandle : NPCHandles)
    {
        if (!EntityManager.IsEntityValid(NPCHandle))
        {
            continue;
        }
        const FArcEconomyNPCFragment* NPC = EntityManager.GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
        if (!NPC)
        {
            continue;
        }
        const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(NPCHandle);
        const FMassEntityView NPCView(EntityManager, NPCHandle);

        FNPCData& Data = OutNPCs.AddDefaulted_GetRef();
        Data.Handle = NPCHandle;
        Data.Role = NPC->Role;
        Data.Location = Transform ? Transform->GetTransform().GetLocation() : FVector::ZeroVector;
        Data.bIsCaravan = NPCView.HasTag<FArcCaravanTag>();
    }
}

void AssignWorkers(
    FGovernorContext& Ctx,
    TArray<FArcGovernorPendingNPCChange>& OutNPCChanges,
    TArray<FArcGovernorPendingSlotChange>& OutSlotChanges)
{
    FMassEntityManager& EntityManager = Ctx.EntityManager;

    // --- Collect idle NPC indices ---
    struct FIdleNPCIndex
    {
        int32 NPCIndex;
        FVector Location;
    };
    TArray<FIdleNPCIndex> IdleNPCs;

    for (int32 Index = 0; Index < Ctx.NPCs.Num(); ++Index)
    {
        const FNPCData& NPC = Ctx.NPCs[Index];
        if (NPC.Role == EArcEconomyNPCRole::Idle)
        {
            IdleNPCs.Add({Index, NPC.Location});
        }
    }

    if (IdleNPCs.Num() == 0)
    {
        return;
    }

    // --- Count workers already assigned per building ---
    TMap<FMassEntityHandle, int32> AssignedWorkerCount;
    for (const FNPCData& NPC : Ctx.NPCs)
    {
        if (NPC.Role != EArcEconomyNPCRole::Worker)
        {
            continue;
        }
        const FArcWorkerFragment* Worker = EntityManager.GetFragmentDataPtr<FArcWorkerFragment>(NPC.Handle);
        if (!Worker || !Worker->AssignedBuildingHandle.IsValid())
        {
            continue;
        }
        int32& Count = AssignedWorkerCount.FindOrAdd(Worker->AssignedBuildingHandle);
        ++Count;
    }

    // --- Build per-building candidate list ---
    struct FBuildingCandidate
    {
        FMassEntityHandle BuildingHandle;
        FVector Location;
        FArcAreaHandle AreaHandle;
        int32 FreeWorkerSlots;
        TArray<int32> VacantAreaSlotIndices;
    };
    TArray<FBuildingCandidate> Candidates;

    const FGameplayTagContainer* WorkerTags = Ctx.Config.SlotRoleMapping.Find(EArcEconomyNPCRole::Worker);

    for (const FMassEntityHandle& BuildingHandle : Ctx.BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }
        const FArcBuildingFragment* Building = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        if (!Building)
        {
            continue;
        }
        if (!Building->bProductionEnabled)
        {
            continue;
        }
        const FArcSmartObjectDefinitionSharedFragment* SOShared =
            EntityManager.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(BuildingHandle);
        if (!SOShared || !SOShared->SmartObjectDefinition)
        {
            continue;
        }

        const FArcBuildingSlotSummary Summary = ClassifyBuildingSlots(
            *SOShared->SmartObjectDefinition,
            Ctx.Config.SlotRoleMapping);

        if (Summary.TotalWorkerSlots <= 0)
        {
            continue;
        }

        const int32 Assigned = AssignedWorkerCount.FindRef(BuildingHandle);
        const int32 FreeSlots = Summary.TotalWorkerSlots - Assigned;
        if (FreeSlots <= 0)
        {
            continue;
        }

        // Backpressure: skip buildings whose output buffer is full
        const FArcBuildingEconomyConfig* EconConfig =
            EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);
        if (EconConfig && Building->CurrentOutputCount >= EconConfig->OutputBufferSize)
        {
            continue;
        }

        // Collect vacant area slot indices for this building
        FArcAreaHandle AreaHandle;
        TArray<int32> VacantSlots;
        const FArcAreaFragment* AreaFrag = EntityManager.GetFragmentDataPtr<FArcAreaFragment>(BuildingHandle);
        if (AreaFrag && AreaFrag->AreaHandle.IsValid() && Ctx.AreaSubsystem)
        {
            AreaHandle = AreaFrag->AreaHandle;
            const FArcAreaData* AreaData = Ctx.AreaSubsystem->GetAreaData(AreaHandle);
            if (AreaData)
            {
                for (int32 SlotIdx = 0; SlotIdx < AreaData->Slots.Num(); ++SlotIdx)
                {
                    const FArcAreaSlotHandle SlotHandle(AreaHandle, SlotIdx);
                    if (Ctx.AreaSubsystem->GetSlotState(SlotHandle) != EArcAreaSlotState::Vacant)
                    {
                        continue;
                    }
                    if (WorkerTags && !AreaData->Slots[SlotIdx].ActivityTags.HasAll(*WorkerTags))
                    {
                        continue;
                    }
                    VacantSlots.Add(SlotIdx);
                }
            }
        }

        FBuildingCandidate Candidate;
        Candidate.BuildingHandle = BuildingHandle;
        Candidate.Location = Building->BuildingLocation;
        Candidate.AreaHandle = AreaHandle;
        Candidate.FreeWorkerSlots = FreeSlots;
        Candidate.VacantAreaSlotIndices = MoveTemp(VacantSlots);
        Candidates.Add(MoveTemp(Candidate));
    }

    if (Candidates.Num() == 0)
    {
        return;
    }

    // Helper: assign nearest idle NPC to a building (including area slot assignment)
    auto AssignNearestIdleNPC = [&](FBuildingCandidate& Building)
    {
        if (IdleNPCs.Num() == 0 || Building.FreeWorkerSlots <= 0)
        {
            return;
        }

        int32 BestIdx = 0;
        float BestDistSq = TNumericLimits<float>::Max();
        for (int32 I = 0; I < IdleNPCs.Num(); ++I)
        {
            const float DistSq = FVector::DistSquared(IdleNPCs[I].Location, Building.Location);
            if (DistSq < BestDistSq)
            {
                BestDistSq = DistSq;
                BestIdx = I;
            }
        }

        const int32 NPCIndex = IdleNPCs[BestIdx].NPCIndex;
        const FMassEntityHandle NPCHandle = Ctx.NPCs[NPCIndex].Handle;

        // Assign NPC to a vacant area slot on the building
        if (Ctx.AreaSubsystem && Building.AreaHandle.IsValid() && Building.VacantAreaSlotIndices.Num() > 0)
        {
            const int32 AreaSlotIdx = Building.VacantAreaSlotIndices.Pop();
            const FArcAreaSlotHandle AreaSlotHandle(Building.AreaHandle, AreaSlotIdx);
            Ctx.AreaSubsystem->AssignToSlot(AreaSlotHandle, NPCHandle);
        }

        // Mark NPC as Worker locally so other phases skip it
        Ctx.NPCs[NPCIndex].Role = EArcEconomyNPCRole::Worker;

        FArcGovernorPendingNPCChange NPCChange;
        NPCChange.NPCHandle = NPCHandle;
        NPCChange.NewRole = EArcEconomyNPCRole::Worker;
        NPCChange.AssignedBuildingHandle = Building.BuildingHandle;

        const FArcBuildingFragment* BuildingFrag = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(Building.BuildingHandle);
        const FArcBuildingEconomyConfig* BuildingEconConfig = EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(Building.BuildingHandle);
        if (BuildingFrag && BuildingEconConfig)
        {
            NPCChange.bBackpressured = BuildingFrag->CurrentOutputCount >= BuildingEconConfig->OutputBufferSize;
        }

        OutNPCChanges.Add(NPCChange);

        --Building.FreeWorkerSlots;
        IdleNPCs.RemoveAtSwap(BestIdx);
    };

    // --- Pass 1 (Spread): give one worker to every building that has none ---
    for (FBuildingCandidate& Candidate : Candidates)
    {
        if (IdleNPCs.Num() == 0)
        {
            return;
        }
        const int32 Assigned = AssignedWorkerCount.FindRef(Candidate.BuildingHandle);
        if (Assigned == 0)
        {
            AssignNearestIdleNPC(Candidate);
        }
    }

    // --- Pass 2 (Fill): fill remaining free slots across all buildings ---
    for (FBuildingCandidate& Candidate : Candidates)
    {
        while (IdleNPCs.Num() > 0 && Candidate.FreeWorkerSlots > 0)
        {
            AssignNearestIdleNPC(Candidate);
        }
        if (IdleNPCs.Num() == 0)
        {
            return;
        }
    }
}

void AssignGatherers(
    FGovernorContext& Ctx,
    TArray<FArcGovernorPendingNPCChange>& OutNPCChanges)
{
    FMassEntityManager& EntityManager = Ctx.EntityManager;

    // --- Collect idle NPC indices ---
    struct FIdleNPCIndex
    {
        int32 NPCIndex;
        FVector Location;
    };
    TArray<FIdleNPCIndex> IdleNPCs;

    for (int32 Index = 0; Index < Ctx.NPCs.Num(); ++Index)
    {
        const FNPCData& NPC = Ctx.NPCs[Index];
        if (NPC.Role == EArcEconomyNPCRole::Idle)
        {
            IdleNPCs.Add({Index, NPC.Location});
        }
    }

    if (IdleNPCs.Num() == 0)
    {
        return;
    }

    // --- Build set of already-occupied resource targets ---
    TSet<FMassEntityHandle> OccupiedResources;
    for (const FNPCData& NPC : Ctx.NPCs)
    {
        if (NPC.Role != EArcEconomyNPCRole::Gatherer)
        {
            continue;
        }
        const FArcGathererFragment* Gatherer = EntityManager.GetFragmentDataPtr<FArcGathererFragment>(NPC.Handle);
        if (Gatherer && Gatherer->TargetResourceHandle.IsValid())
        {
            OccupiedResources.Add(Gatherer->TargetResourceHandle);
        }
    }

    // --- Count gatherers already assigned per building ---
    TMap<FMassEntityHandle, int32> AssignedGathererCount;
    for (const FNPCData& NPC : Ctx.NPCs)
    {
        if (NPC.Role != EArcEconomyNPCRole::Gatherer)
        {
            continue;
        }
        const FArcGathererFragment* Gatherer = EntityManager.GetFragmentDataPtr<FArcGathererFragment>(NPC.Handle);
        if (!Gatherer || !Gatherer->AssignedBuildingHandle.IsValid())
        {
            continue;
        }
        int32& Count = AssignedGathererCount.FindOrAdd(Gatherer->AssignedBuildingHandle);
        ++Count;
    }

    // --- Build per-building candidate list ---
    struct FGatherBuildingCandidate
    {
        FMassEntityHandle BuildingHandle;
        FVector Location;
        int32 FreeSlots;
        FGameplayTagContainer SearchTags;
        float SearchRadius;
    };
    TArray<FGatherBuildingCandidate> Candidates;

    for (const FMassEntityHandle& BuildingHandle : Ctx.BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }
        const FArcBuildingFragment* Building = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        const FArcBuildingEconomyConfig* EconConfig =
            EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);
        if (!Building || !EconConfig || !EconConfig->IsGatheringBuilding())
        {
            continue;
        }
        if (!Building->bProductionEnabled)
        {
            continue;
        }

        // Backpressure: skip buildings whose output buffer is full
        if (Building->CurrentOutputCount >= EconConfig->OutputBufferSize)
        {
            continue;
        }

        const int32 Assigned = AssignedGathererCount.FindRef(BuildingHandle);
        const int32 FreeSlots = EconConfig->MaxProductionSlots - Assigned;
        if (FreeSlots <= 0)
        {
            continue;
        }

        FGatherBuildingCandidate Candidate;
        Candidate.BuildingHandle = BuildingHandle;
        Candidate.Location = Building->BuildingLocation;
        Candidate.FreeSlots = FreeSlots;
        Candidate.SearchTags = EconConfig->GatherSearchTags;
        Candidate.SearchRadius = EconConfig->GatherSearchRadius;
        Candidates.Add(MoveTemp(Candidate));
    }

    if (Candidates.Num() == 0)
    {
        return;
    }

    // --- Assign gatherers to buildings ---
    for (FGatherBuildingCandidate& Candidate : Candidates)
    {
        while (IdleNPCs.Num() > 0 && Candidate.FreeSlots > 0)
        {
            // Find resource target via spatial hash
            FMassEntityHandle TargetResource;
            if (Ctx.SpatialHashSubsystem && !Candidate.SearchTags.IsEmpty())
            {
                // Query for each search tag and pick the first unoccupied result
                for (const FGameplayTag& Tag : Candidate.SearchTags)
                {
                    TArray<FArcMassEntityInfo> Results;
                    Ctx.SpatialHashSubsystem->QuerySphere(ArcSpatialHash::HashKey(Tag), Candidate.Location, Candidate.SearchRadius, Results);

                    for (const FArcMassEntityInfo& Result : Results)
                    {
                        if (!OccupiedResources.Contains(Result.Entity))
                        {
                            TargetResource = Result.Entity;
                            break;
                        }
                    }
                    if (TargetResource.IsValid())
                    {
                        break;
                    }
                }
            }

            if (!TargetResource.IsValid())
            {
                break; // No resources available for this building
            }

            // Find nearest idle NPC
            int32 BestIdx = 0;
            float BestDistSq = TNumericLimits<float>::Max();
            for (int32 I = 0; I < IdleNPCs.Num(); ++I)
            {
                const float DistSq = FVector::DistSquared(IdleNPCs[I].Location, Candidate.Location);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    BestIdx = I;
                }
            }

            const int32 NPCIndex = IdleNPCs[BestIdx].NPCIndex;
            const FMassEntityHandle NPCHandle = Ctx.NPCs[NPCIndex].Handle;

            // Mark NPC as Gatherer locally
            Ctx.NPCs[NPCIndex].Role = EArcEconomyNPCRole::Gatherer;

            FArcGovernorPendingNPCChange NPCChange;
            NPCChange.NPCHandle = NPCHandle;
            NPCChange.NewRole = EArcEconomyNPCRole::Gatherer;
            NPCChange.AssignedBuildingHandle = Candidate.BuildingHandle;
            NPCChange.GathererTargetResource = TargetResource;
            OutNPCChanges.Add(NPCChange);

            OccupiedResources.Add(TargetResource);
            --Candidate.FreeSlots;
            IdleNPCs.RemoveAtSwap(BestIdx);
        }

        if (IdleNPCs.Num() == 0)
        {
            return;
        }
    }
}

void RebalanceTransporters(
    FGovernorContext& Ctx,
    TArray<FArcGovernorPendingNPCChange>& OutNPCChanges,
    TArray<FArcGovernorPendingWorkforceUpdate>& OutWorkforceUpdates)
{
    FMassEntityManager& EntityManager = Ctx.EntityManager;

    int32 TotalOutputBacklog = 0;
    for (const FMassEntityHandle& BuildingHandle : Ctx.BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }
        const FArcCraftOutputFragment* Output = EntityManager.GetFragmentDataPtr<FArcCraftOutputFragment>(BuildingHandle);
        if (Output)
        {
            for (const FArcItemSpec& Item : Output->OutputItems)
            {
                TotalOutputBacklog += Item.Amount;
            }
        }
    }

    // Count total transport slots across all buildings
    int32 TotalTransportSlots = 0;
    for (const FMassEntityHandle& BuildingHandle : Ctx.BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }
        const FArcSmartObjectDefinitionSharedFragment* SODef =
            EntityManager.GetConstSharedFragmentDataPtr<FArcSmartObjectDefinitionSharedFragment>(BuildingHandle);
        if (!SODef || !SODef->SmartObjectDefinition)
        {
            continue;
        }
        const FArcBuildingSlotSummary Summary = ClassifyBuildingSlots(
            *SODef->SmartObjectDefinition, Ctx.Config.SlotRoleMapping);
        TotalTransportSlots += Summary.TotalTransportSlots;
    }

    int32 WorkerCount = 0;
    int32 LocalTransporterCount = 0;
    int32 GathererCount = 0;
    int32 CaravanCount = 0;
    int32 IdleCount = 0;

    for (const FNPCData& NPC : Ctx.NPCs)
    {
        switch (NPC.Role)
        {
        case EArcEconomyNPCRole::Worker:
            ++WorkerCount;
            break;
        case EArcEconomyNPCRole::Transporter:
            if (NPC.bIsCaravan)
            {
                ++CaravanCount;
            }
            else
            {
                ++LocalTransporterCount;
            }
            break;
        case EArcEconomyNPCRole::Gatherer:
            ++GathererCount;
            break;
        case EArcEconomyNPCRole::Idle:
            ++IdleCount;
            break;
        }
    }

    const int32 FreeTransportSlots = FMath::Max(0, TotalTransportSlots - LocalTransporterCount);

    FArcGovernorPendingWorkforceUpdate& OutWorkforceUpdate = OutWorkforceUpdates.AddDefaulted_GetRef();
    OutWorkforceUpdate.SettlementHandle = Ctx.SettlementEntity;
    OutWorkforceUpdate.WorkerCount = WorkerCount;
    OutWorkforceUpdate.TransporterCount = LocalTransporterCount;
    OutWorkforceUpdate.GathererCount = GathererCount;
    OutWorkforceUpdate.CaravanCount = CaravanCount;
    OutWorkforceUpdate.IdleCount = IdleCount;

    const bool bBacklogged = TotalOutputBacklog > Ctx.Config.OutputBacklogThreshold;
    const float CurrentRatio = LocalTransporterCount > 0
        ? static_cast<float>(WorkerCount) / static_cast<float>(LocalTransporterCount)
        : TNumericLimits<float>::Max();
    const bool bNeedMoreTransporters = bBacklogged || CurrentRatio > Ctx.Config.TargetWorkerTransporterRatio * 1.5f;

    if (bNeedMoreTransporters && IdleCount > 0 && FreeTransportSlots > 0)
    {
        for (FNPCData& NPC : Ctx.NPCs)
        {
            if (NPC.Role == EArcEconomyNPCRole::Idle)
            {
                NPC.Role = EArcEconomyNPCRole::Transporter;

                FArcGovernorPendingNPCChange Change;
                Change.NPCHandle = NPC.Handle;
                Change.NewRole = EArcEconomyNPCRole::Transporter;
                Change.TransporterState = EArcTransporterTaskState::Idle;
                OutNPCChanges.Add(Change);
                break;
            }
        }
    }

    if (bBacklogged && LocalTransporterCount == 0 && CaravanCount > 0)
    {
        for (const FNPCData& NPC : Ctx.NPCs)
        {
            if (NPC.Role == EArcEconomyNPCRole::Transporter && NPC.bIsCaravan)
            {
                FArcGovernorPendingNPCChange Change;
                Change.NPCHandle = NPC.Handle;
                Change.NewRole = EArcEconomyNPCRole::Transporter;
                Change.TransporterState = EArcTransporterTaskState::Idle;
                OutNPCChanges.Add(Change);
                break;
            }
        }
    }
}

float ComputeNeedScore(const FGovernorContext& Ctx, const UArcItemDefinition* OutputItem)
{
    if (!Ctx.NeedsConfig || !OutputItem)
    {
        return 1.0f;
    }

    const FArcItemFragment_Tags* ItemTags = OutputItem->FindFragment<FArcItemFragment_Tags>();
    if (!ItemTags)
    {
        return 1.0f;
    }

    float BestScore = 1.0f;

    for (const TPair<TObjectPtr<UScriptStruct>, TObjectPtr<UArcSettlementNeedDataAsset>>& Pair : Ctx.NeedsConfig->NeedConfigs)
    {
        const UArcSettlementNeedDataAsset* DataAsset = Pair.Value;
        if (!DataAsset)
        {
            continue;
        }

        for (const FArcNeedSatisfactionEntry& Entry : DataAsset->SatisfactionSources)
        {
            if (!ItemTags->AssetTags.HasTag(Entry.ItemTag))
            {
                continue;
            }

            const float* NeedValue = Ctx.NeedValues.Find(Pair.Key);
            if (!NeedValue)
            {
                continue;
            }

            float Score = *NeedValue; // 0-100, higher = more urgent
            if (DataAsset->SatisfactionCurve)
            {
                Score = DataAsset->SatisfactionCurve->GetFloatValue(*NeedValue);
            }

            BestScore = FMath::Max(BestScore, Score);
            break; // One match per need is enough
        }
    }

    return BestScore;
}

void ActivateRecipesFromDemand(
    FGovernorContext& Ctx,
    TArray<FArcGovernorPendingSlotChange>& OutSlotChanges)
{
    FMassEntityManager& EntityManager = Ctx.EntityManager;

    // --- Part 1: Collect scored candidates from Potential producer nodes ---
    bool bAnyActivated = false;

    struct FScoredCandidate
    {
        FMassEntityHandle BuildingHandle;
        int32 SlotIndex;
        UArcRecipeDefinition* Recipe;
        float NeedScore;
    };
    TArray<FScoredCandidate> Candidates;

    for (const ArcDemandGraph::FNode& Node : Ctx.DemandGraph.Nodes)
    {
        if (Node.Type != ArcDemandGraph::ENodeType::Producer)
        {
            continue;
        }
        if (Node.Status != ArcDemandGraph::ENodeStatus::Potential)
        {
            continue;
        }

        // Skip if settlement storage is full
        if (Ctx.Market.CurrentTotalStorage >= Ctx.Market.TotalStorageCap)
        {
            continue;
        }

        const FMassEntityHandle BuildingHandle = Node.Entity;
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        const FArcBuildingFragment* BuildingFrag =
            EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        if (!BuildingFrag || !BuildingFrag->bProductionEnabled)
        {
            continue;
        }

        FArcBuildingEconomyConfig* EconomyConfig =
            EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);
        const FArcBuildingWorkforceFragment* Workforce =
            EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(BuildingHandle);
        if (!EconomyConfig || !Workforce)
        {
            continue;
        }

        const TArray<UArcRecipeDefinition*>& Recipes = ArcEconomy::ResolveAllowedRecipes(*EconomyConfig);
        if (Recipes.Num() == 0)
        {
            continue;
        }

        // Find the recipe that produces Node.ItemDef
        UArcRecipeDefinition* MatchingRecipe = nullptr;
        for (UArcRecipeDefinition* Recipe : Recipes)
        {
            if (!Recipe || !Recipe->OutputItemDefinition.IsValid())
            {
                continue;
            }
            UArcItemDefinition* OutputItem =
                UArcCoreAssetManager::GetAsset<UArcItemDefinition>(Recipe->OutputItemDefinition.AssetId);
            if (OutputItem == Node.ItemDef)
            {
                MatchingRecipe = Recipe;
                break;
            }
        }

        if (!MatchingRecipe)
        {
            continue;
        }

        // Find an empty workforce slot (DesiredRecipe == nullptr)
        for (int32 SlotIndex = 0; SlotIndex < Workforce->Slots.Num(); ++SlotIndex)
        {
            const FArcBuildingSlotData& Slot = Workforce->Slots[SlotIndex];
            if (Slot.DesiredRecipe != nullptr)
            {
                continue;
            }

            const float NeedScore = ComputeNeedScore(Ctx, Node.ItemDef);
            Candidates.Add({BuildingHandle, SlotIndex, MatchingRecipe, NeedScore});
            break;
        }
    }

    // Sort candidates by need score descending (most urgent needs first)
    Candidates.Sort([](const FScoredCandidate& A, const FScoredCandidate& B)
    {
        return A.NeedScore > B.NeedScore;
    });

    // Emit slot changes in priority order
    for (const FScoredCandidate& Candidate : Candidates)
    {
        FArcGovernorPendingSlotChange Change;
        Change.BuildingHandle = Candidate.BuildingHandle;
        Change.SlotIndex = Candidate.SlotIndex;
        Change.NewDesiredRecipe = Candidate.Recipe;
        OutSlotChanges.Add(Change);
        bAnyActivated = true;
    }

    // --- Part 2: Fallback — no consumer seeds existed, activate baseline production ---
    // If no Potential producer nodes were activated and the graph has no consumer nodes
    // (only the settlement root), assign the first available recipe to any building with
    // empty workforce slots so production starts without explicit demand seeds.
    if (bAnyActivated || Ctx.DemandGraph.Nodes.Num() > 1)
    {
        return;
    }

    // Settlement storage full — nothing to do
    if (Ctx.Market.CurrentTotalStorage >= Ctx.Market.TotalStorageCap)
    {
        return;
    }

    for (const FMassEntityHandle& BuildingHandle : Ctx.BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        const FArcBuildingFragment* BuildingFrag =
            EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        if (!BuildingFrag || !BuildingFrag->bProductionEnabled)
        {
            continue;
        }

        FArcBuildingEconomyConfig* EconomyConfig =
            EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);
        const FArcBuildingWorkforceFragment* Workforce =
            EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(BuildingHandle);
        if (!EconomyConfig || !Workforce)
        {
            continue;
        }

        const TArray<UArcRecipeDefinition*>& Recipes = ArcEconomy::ResolveAllowedRecipes(*EconomyConfig);
        if (Recipes.Num() == 0)
        {
            continue;
        }

        // Find an empty slot and assign the first available recipe
        for (int32 SlotIndex = 0; SlotIndex < Workforce->Slots.Num(); ++SlotIndex)
        {
            const FArcBuildingSlotData& Slot = Workforce->Slots[SlotIndex];
            if (Slot.DesiredRecipe != nullptr)
            {
                continue;
            }

            UArcRecipeDefinition* FirstRecipe = Recipes[0];
            if (!FirstRecipe)
            {
                break;
            }

            FArcGovernorPendingSlotChange Change;
            Change.BuildingHandle = BuildingHandle;
            Change.SlotIndex = SlotIndex;
            Change.NewDesiredRecipe = FirstRecipe;
            OutSlotChanges.Add(Change);
            break;
        }
    }
}

void DeactivateIdleRecipes(
    FGovernorContext& Ctx,
    TArray<FArcGovernorPendingSlotChange>& OutSlotChanges)
{
    FMassEntityManager& EntityManager = Ctx.EntityManager;

    // Build set of items that are currently demanded by consumers
    TSet<TObjectPtr<UArcItemDefinition>> DemandedItems;
    for (const ArcDemandGraph::FNode& Node : Ctx.DemandGraph.Nodes)
    {
        if (Node.Type == ArcDemandGraph::ENodeType::Consumer && Node.ItemDef)
        {
            DemandedItems.Add(Node.ItemDef);
        }
    }

    for (const FMassEntityHandle& BuildingHandle : Ctx.BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }

        const FArcBuildingFragment* Building =
            EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        const FArcBuildingWorkforceFragment* Workforce =
            EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(BuildingHandle);
        const FArcBuildingEconomyConfig* EconomyConfig =
            EntityManager.GetSharedFragmentDataPtr<FArcBuildingEconomyConfig>(BuildingHandle);
        if (!Building || !Workforce || !EconomyConfig)
        {
            continue;
        }

        for (int32 SlotIndex = 0; SlotIndex < Workforce->Slots.Num(); ++SlotIndex)
        {
            const FArcBuildingSlotData& Slot = Workforce->Slots[SlotIndex];
            if (!Slot.DesiredRecipe)
            {
                continue;
            }

            // Check if the recipe output is still demanded
            if (Slot.DesiredRecipe->OutputItemDefinition.IsValid())
            {
                UArcItemDefinition* OutputItem =
                    UArcCoreAssetManager::GetAsset<UArcItemDefinition>(Slot.DesiredRecipe->OutputItemDefinition.AssetId);
                if (DemandedItems.Contains(OutputItem))
                {
                    continue;
                }
            }

            // Not demanded and output buffer is full — deactivate
            if (Building->CurrentOutputCount >= EconomyConfig->OutputBufferSize)
            {
                FArcGovernorPendingSlotChange Change;
                Change.BuildingHandle = BuildingHandle;
                Change.SlotIndex = SlotIndex;
                Change.NewDesiredRecipe = nullptr;
                OutSlotChanges.Add(Change);
            }
        }
    }
}

void EstablishTradeRoutes(
    const FGovernorContext& Ctx,
    const FArcSettlementWorkforceFragment& Workforce,
    UArcKnowledgeSubsystem& KnowledgeSub,
    TArray<FArcGovernorPendingNPCChange>& OutNPCChanges)
{
    if (Workforce.TransporterCount == 0 && Workforce.IdleCount == 0)
    {
        return;
    }

    FGameplayTagQuery MarketTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(ArcEconomy::Tags::TAG_Knowledge_Economy_Market);
    TArray<FArcKnowledgeHandle> MarketHandles;
    KnowledgeSub.QueryKnowledgeInRadius(Ctx.SettlementLocation, 100000.0f, MarketHandles, MarketTagQuery);

    float BestDifferential = 0.0f;

    for (const FArcKnowledgeHandle& MarketEntryHandle : MarketHandles)
    {
        const FArcKnowledgeEntry* MarketEntry = KnowledgeSub.GetKnowledgeEntry(MarketEntryHandle);
        if (!MarketEntry || MarketEntry->SourceEntity == Ctx.SettlementEntity)
        {
            continue;
        }

        const FArcEconomyMarketPayload* NeighborMarket = MarketEntry->Payload.GetPtr<FArcEconomyMarketPayload>();
        if (!NeighborMarket)
        {
            continue;
        }

        for (const TPair<TObjectPtr<UArcItemDefinition>, float>& NeighborPrice : NeighborMarket->PriceSnapshot)
        {
            const FArcResourceMarketData* OurData = Ctx.Market.PriceTable.Find(NeighborPrice.Key);
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

    if (BestDifferential >= Ctx.Config.TradePriceDifferentialThreshold && Workforce.IdleCount > 0)
    {
        for (const FNPCData& NPC : Ctx.NPCs)
        {
            if (NPC.Role == EArcEconomyNPCRole::Idle && NPC.bIsCaravan)
            {
                FArcGovernorPendingNPCChange Change;
                Change.NPCHandle = NPC.Handle;
                Change.NewRole = EArcEconomyNPCRole::Transporter;
                Change.TransporterState = EArcTransporterTaskState::Idle;
                OutNPCChanges.Add(Change);
                break;
            }
        }
    }
}

} // namespace ArcGovernor
