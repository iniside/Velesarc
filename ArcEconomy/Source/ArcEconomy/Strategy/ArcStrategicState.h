// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ArcStrategicState.generated.h"

struct FMassEntityManager;
struct FArcSettlementFragment;
struct FArcSettlementMarketFragment;
struct FArcSettlementWorkforceFragment;
struct FArcFactionFragment;
struct FArcFactionDiplomacyFragment;
struct FArcFactionSettlementsFragment;
class UArcKnowledgeSubsystem;
class UArcSettlementSubsystem;

/**
 * Normalized strategic state for a single settlement. All values 0–100.
 */
USTRUCT()
struct ARCECONOMY_API FArcSettlementStrategicState
{
    GENERATED_BODY()

    float Security = 50.0f;    // 0 = defenseless, 100 = fortress
    float Prosperity = 50.0f;  // 0 = nothing running, 100 = fully utilized
    float Growth = 50.0f;      // 0 = no room, 100 = lots of expansion potential
    float Labor = 50.0f;       // 0 = fully staffed, 100 = desperate for workers
    float Military = 50.0f;    // 0 = full garrison, 100 = need soldiers

    /** Per-resource need scores from governor ComputeNeedScore(). Padded to fixed size for ML. */
    TMap<FGameplayTag, float> ResourceScores;
};

/**
 * Normalized strategic state for a faction. All values 0–100.
 */
USTRUCT()
struct ARCECONOMY_API FArcFactionStrategicState
{
    GENERATED_BODY()

    float Dominance = 50.0f;   // 0 = minor, 100 = dominant power
    float Stability = 50.0f;   // 0 = collapsing, 100 = secure
    float Wealth = 50.0f;      // 0 = destitute, 100 = rich
    float Expansion = 50.0f;   // 0 = no opportunity, 100 = unclaimed riches nearby
};

/**
 * Functions to compute strategic state from world data.
 */
namespace ArcStrategy::StateComputation
{
    ARCECONOMY_API FArcSettlementStrategicState ComputeSettlementState(
        FMassEntityManager& EntityManager,
        FMassEntityHandle SettlementEntity,
        const FArcSettlementFragment& Settlement,
        const FArcSettlementMarketFragment& Market,
        const FArcSettlementWorkforceFragment& Workforce,
        UArcKnowledgeSubsystem& KnowledgeSub,
        UArcSettlementSubsystem& SettlementSub);

    ARCECONOMY_API FArcFactionStrategicState ComputeFactionState(
        FMassEntityManager& EntityManager,
        FMassEntityHandle FactionEntity,
        const FArcFactionFragment& Faction,
        const FArcFactionDiplomacyFragment& Diplomacy,
        const FArcFactionSettlementsFragment& Settlements,
        const TMap<FMassEntityHandle, FArcSettlementStrategicState>& SettlementStates);
}
