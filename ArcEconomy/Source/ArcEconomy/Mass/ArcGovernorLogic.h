// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"
#include "ArcEconomyTypes.h"
#include "Mass/ArcDemandGraph.h"

struct FMassEntityManager;
class UArcGovernorDataAsset;
class USmartObjectDefinition;
class UArcKnowledgeSubsystem;
class UArcRecipeDefinition;
class UArcAreaSubsystem;
class UArcMassSpatialHashSubsystem;
struct FArcSettlementFragment;
struct FArcSettlementMarketFragment;
struct FArcSettlementWorkforceFragment;
struct FArcSettlementNeedsConfigFragment;

namespace ArcGovernor
{
    struct FArcBuildingSlotSummary
    {
        int32 TotalWorkerSlots = 0;
        int32 TotalTransportSlots = 0;
        int32 TotalGathererSlots = 0;
        int32 FreeWorkerSlots = 0;
        int32 FreeTransportSlots = 0;
        int32 FreeGathererSlots = 0;
    };

    struct FArcGovernorPendingNPCChange
    {
        FMassEntityHandle NPCHandle;
        EArcEconomyNPCRole NewRole = EArcEconomyNPCRole::Idle;
        FMassEntityHandle AssignedBuildingHandle;
        EArcTransporterTaskState TransporterState = EArcTransporterTaskState::Idle;
        bool bBackpressured = false;
        FMassEntityHandle GathererTargetResource; // Only used when NewRole == Gatherer
    };

    struct FArcGovernorPendingSlotChange
    {
        FMassEntityHandle BuildingHandle;
        int32 SlotIndex = INDEX_NONE;
        TObjectPtr<UArcRecipeDefinition> NewDesiredRecipe = nullptr;
    };

    struct FArcGovernorPendingWorkforceUpdate
    {
        FMassEntityHandle SettlementHandle;
        int32 WorkerCount = 0;
        int32 TransporterCount = 0;
        int32 GathererCount = 0;
        int32 CaravanCount = 0;
        int32 IdleCount = 0;
    };

    /** Pre-fetched NPC data to avoid repeated GetFragmentDataPtr calls across governor phases. */
    struct FNPCData
    {
        FMassEntityHandle Handle;
        EArcEconomyNPCRole Role = EArcEconomyNPCRole::Idle;
        FVector Location = FVector::ZeroVector;
        bool bIsCaravan = false;
    };

    struct FGovernorContext
    {
        FMassEntityManager& EntityManager;
        FMassEntityHandle SettlementEntity;
        FVector SettlementLocation;
        const FArcSettlementFragment& Settlement;
        const FArcSettlementMarketFragment& Market;
        const UArcGovernorDataAsset& Config;
        TArray<FMassEntityHandle> BuildingHandles;
        TArray<FNPCData> NPCs;
        UArcAreaSubsystem* AreaSubsystem;
        UArcMassSpatialHashSubsystem* SpatialHashSubsystem;
        ArcDemandGraph::FDemandGraph DemandGraph;

        /** Settlement need values snapshot. Empty if no needs configured. */
        TMap<UScriptStruct*, float> NeedValues;

        /** Settlement needs config. nullptr if not configured. */
        const FArcSettlementNeedsConfigFragment* NeedsConfig = nullptr;
    };

    /** Pre-fetch NPC fragment data in a single pass. */
    void PreFetchNPCs(FMassEntityManager& EntityManager, const TArray<FMassEntityHandle>& NPCHandles, TArray<FNPCData>& OutNPCs);

    ARCECONOMY_API FArcBuildingSlotSummary ClassifyBuildingSlots(
        const USmartObjectDefinition& SODefinition,
        const TMap<EArcEconomyNPCRole, FGameplayTagContainer>& SlotRoleMapping);

    void AssignWorkers(
        FGovernorContext& Ctx,
        TArray<FArcGovernorPendingNPCChange>& OutNPCChanges,
        TArray<FArcGovernorPendingSlotChange>& OutSlotChanges);

    void AssignGatherers(
        FGovernorContext& Ctx,
        TArray<FArcGovernorPendingNPCChange>& OutNPCChanges);

    void RebalanceTransporters(
        FGovernorContext& Ctx,
        TArray<FArcGovernorPendingNPCChange>& OutNPCChanges,
        TArray<FArcGovernorPendingWorkforceUpdate>& OutWorkforceUpdates);

    /** Compute need-based priority score for a production chain output item.
     *  Returns 1.0 (neutral) if no needs data is available. Higher = more urgent. */
    float ComputeNeedScore(const FGovernorContext& Ctx, const UArcItemDefinition* OutputItem);

    void ActivateRecipesFromDemand(
        FGovernorContext& Ctx,
        TArray<FArcGovernorPendingSlotChange>& OutSlotChanges);

    void DeactivateIdleRecipes(
        FGovernorContext& Ctx,
        TArray<FArcGovernorPendingSlotChange>& OutSlotChanges);

    void EstablishTradeRoutes(
        const FGovernorContext& Ctx,
        const FArcSettlementWorkforceFragment& Workforce,
        UArcKnowledgeSubsystem& KnowledgeSub,
        TArray<FArcGovernorPendingNPCChange>& OutNPCChanges);
}
