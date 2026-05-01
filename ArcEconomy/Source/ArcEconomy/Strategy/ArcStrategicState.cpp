// Copyright Lukasz Baran. All Rights Reserved.

#include "Strategy/ArcStrategicState.h"
#include "Mass/ArcEconomyFragments.h"
#include "Faction/ArcFactionFragments.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcSettlementSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"

namespace ArcStrategy::StateComputation
{
    FArcSettlementStrategicState ComputeSettlementState(
        FMassEntityManager& EntityManager,
        FMassEntityHandle SettlementEntity,
        const FArcSettlementFragment& Settlement,
        const FArcSettlementMarketFragment& Market,
        const FArcSettlementWorkforceFragment& Workforce,
        UArcKnowledgeSubsystem& KnowledgeSub,
        UArcSettlementSubsystem& SettlementSub)
    {
        FArcSettlementStrategicState State;

        // --- Prosperity: production utilization and storage ---
        const int32 TotalWorkers = Workforce.WorkerCount + Workforce.TransporterCount + Workforce.GathererCount;
        const int32 TotalPopulation = TotalWorkers + Workforce.IdleCount + Workforce.CaravanCount;

        if (TotalPopulation > 0)
        {
            const float UtilizationRatio = static_cast<float>(TotalWorkers) / static_cast<float>(TotalPopulation);
            const float StorageRatio = (Market.TotalStorageCap > 0)
                ? static_cast<float>(Market.CurrentTotalStorage) / static_cast<float>(Market.TotalStorageCap)
                : 0.0f;
            State.Prosperity = FMath::Clamp((UtilizationRatio * 60.0f) + (StorageRatio * 40.0f), 0.0f, 100.0f);
        }
        else
        {
            State.Prosperity = 0.0f;
        }

        // --- Labor: idle ratio inverted (100 = desperate for workers) ---
        if (TotalPopulation > 0)
        {
            const float IdleRatio = static_cast<float>(Workforce.IdleCount) / static_cast<float>(TotalPopulation);
            State.Labor = FMath::Clamp((1.0f - IdleRatio) * 100.0f, 0.0f, 100.0f);
        }
        else
        {
            State.Labor = 100.0f;
        }

        // --- Growth: available capacity ---
        const float CapacityUsed = (Market.TotalStorageCap > 0)
            ? static_cast<float>(Market.CurrentTotalStorage) / static_cast<float>(Market.TotalStorageCap)
            : 1.0f;
        State.Growth = FMath::Clamp((1.0f - CapacityUsed) * 100.0f, 0.0f, 100.0f);

        // --- Security and Military: placeholder until combat system exists ---
        State.Security = 50.0f;
        State.Military = 50.0f;

        return State;
    }

    FArcFactionStrategicState ComputeFactionState(
        FMassEntityManager& EntityManager,
        FMassEntityHandle FactionEntity,
        const FArcFactionFragment& Faction,
        const FArcFactionDiplomacyFragment& Diplomacy,
        const FArcFactionSettlementsFragment& Settlements,
        const TMap<FMassEntityHandle, FArcSettlementStrategicState>& SettlementStates)
    {
        FArcFactionStrategicState State;

        const int32 SettlementCount = Settlements.OwnedSettlements.Num();

        // --- Dominance: settlement count normalized (10 = 100) ---
        State.Dominance = FMath::Clamp(static_cast<float>(SettlementCount) * 10.0f, 0.0f, 100.0f);

        // --- Stability: average Security minus war penalty ---
        float TotalSecurity = 0.0f;
        int32 ValidCount = 0;
        for (const FMassEntityHandle& SettlementHandle : Settlements.OwnedSettlements)
        {
            const FArcSettlementStrategicState* SettState = SettlementStates.Find(SettlementHandle);
            if (SettState)
            {
                TotalSecurity += SettState->Security;
                ++ValidCount;
            }
        }

        int32 ActiveWars = 0;
        for (const TPair<FMassEntityHandle, EArcDiplomaticStance>& Pair : Diplomacy.Stances)
        {
            if (Pair.Value == EArcDiplomaticStance::Hostile)
            {
                ++ActiveWars;
            }
        }

        const float AvgSecurity = (ValidCount > 0) ? (TotalSecurity / static_cast<float>(ValidCount)) : 50.0f;
        const float WarPenalty = FMath::Min(static_cast<float>(ActiveWars) * 15.0f, 50.0f);
        State.Stability = FMath::Clamp(AvgSecurity - WarPenalty, 0.0f, 100.0f);

        // --- Wealth: average Prosperity ---
        float TotalProsperity = 0.0f;
        ValidCount = 0;
        for (const FMassEntityHandle& SettlementHandle : Settlements.OwnedSettlements)
        {
            const FArcSettlementStrategicState* SettState = SettlementStates.Find(SettlementHandle);
            if (SettState)
            {
                TotalProsperity += SettState->Prosperity;
                ++ValidCount;
            }
        }
        State.Wealth = (ValidCount > 0) ? FMath::Clamp(TotalProsperity / static_cast<float>(ValidCount), 0.0f, 100.0f) : 0.0f;

        // --- Expansion: average Growth ---
        float TotalGrowth = 0.0f;
        ValidCount = 0;
        for (const FMassEntityHandle& SettlementHandle : Settlements.OwnedSettlements)
        {
            const FArcSettlementStrategicState* SettState = SettlementStates.Find(SettlementHandle);
            if (SettState)
            {
                TotalGrowth += SettState->Growth;
                ++ValidCount;
            }
        }
        State.Expansion = (ValidCount > 0) ? FMath::Clamp(TotalGrowth / static_cast<float>(ValidCount), 0.0f, 100.0f) : 0.0f;

        return State;
    }
}
