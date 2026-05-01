// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "GameplayTagContainer.h"

class UArcItemDefinition;
class UArcRecipeDefinition;
struct FMassEntityManager;
struct FArcSettlementMarketFragment;
struct FArcBuildingEconomyConfig;

namespace ArcDemandGraph
{
    enum class ENodeType : uint8
    {
        Consumer,
        Producer,
        Settlement
    };

    enum class ENodeStatus : uint8
    {
        Flowing,
        Backpressured,
        Unstaffed,
        UnmetDemand,
        Potential,   // could produce but DesiredRecipe not yet set
        Idle
    };

    struct FNode
    {
        ENodeType Type = ENodeType::Consumer;
        ENodeStatus Status = ENodeStatus::Idle;
        FMassEntityHandle Entity;
        FName BuildingName;
        TObjectPtr<UArcItemDefinition> ItemDef = nullptr;
        FGameplayTag ItemTag;
        int32 DemandQuantity = 0;
        int32 CurrentStock = 0;
        int32 StorageCap = 0;
        bool bBackpressured = false;
        bool bStaffed = true;
    };

    struct FEdge
    {
        int32 ConsumerIdx = INDEX_NONE;
        int32 ProducerIdx = INDEX_NONE;
        int32 DemandQuantity = 0;
    };

    struct FUnmetDemandEntry
    {
        TObjectPtr<UArcItemDefinition> ItemDef = nullptr;
        FGameplayTag ItemTag;
        int32 TotalDemand = 0;
        TArray<int32> ConsumerNodeIndices;
    };

    struct FBottleneckEntry
    {
        int32 ProducerNodeIdx = INDEX_NONE;
        ENodeStatus Reason = ENodeStatus::Idle;
    };

    /** A building that could produce an item via one of its AllowedRecipes, but DesiredRecipe is not yet set. */
    struct FPotentialProducer
    {
        FMassEntityHandle BuildingHandle;
        TObjectPtr<UArcRecipeDefinition> Recipe = nullptr;
        FName BuildingName;
    };

    /** Key for cycle detection during demand-pull BFS: one visited (building, recipe) pair. */
    struct FChainLink
    {
        FMassEntityHandle BuildingHandle;
        TObjectPtr<UArcRecipeDefinition> Recipe = nullptr;

        bool operator==(const FChainLink& Other) const
        {
            return BuildingHandle == Other.BuildingHandle && Recipe == Other.Recipe;
        }
    };

    FORCEINLINE uint32 GetTypeHash(const FChainLink& Key)
    {
        return HashCombine(GetTypeHash(Key.BuildingHandle), GetTypeHash(Key.Recipe));
    }

    struct ARCECONOMY_API FDemandGraph
    {
        TArray<FNode> Nodes;
        TArray<FEdge> Edges;

        void Rebuild(
            FMassEntityManager& EntityManager,
            FMassEntityHandle SettlementEntity,
            const TArray<FMassEntityHandle>& BuildingHandles,
            const FArcSettlementMarketFragment& Market);

        void FindUnmetDemand(TArray<FUnmetDemandEntry>& OutUnmet) const;
        void FindBottlenecks(TArray<FBottleneckEntry>& OutBottlenecks) const;
        void Reset();

    private:
        /** Index built in Pass 1: maps output item def to buildings that could produce it. */
        TMap<TObjectPtr<UArcItemDefinition>, TArray<FPotentialProducer>> PotentialProducerIndex;

        /** Cycle detection: (BuildingHandle, Recipe) pairs already visited during BFS. */
        TSet<FChainLink> VisitedChainLinks;

        /** Find or create an active producer node for a building that already has DesiredRecipe outputting ItemDef.
         *  Returns INDEX_NONE if none found. */
        int32 FindOrCreateActiveProducer(
            FMassEntityManager& EntityManager,
            UArcItemDefinition* ItemDef,
            const TArray<FMassEntityHandle>& BuildingHandles);

        /** Create a potential producer node and enqueue its input ingredients as new consumer nodes.
         *  Returns the index of the created producer node. */
        int32 CreatePotentialProducerAndEnqueueInputs(
            FMassEntityManager& EntityManager,
            const FPotentialProducer& Producer,
            TArray<int32>& BFSQueue);
    };
}
