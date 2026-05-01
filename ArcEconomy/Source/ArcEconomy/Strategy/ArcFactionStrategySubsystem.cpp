// Copyright Lukasz Baran. All Rights Reserved.

#include "Strategy/ArcFactionStrategySubsystem.h"
#include "Strategy/ArcStrategyScoring.h"
#include "HAL/PlatformTime.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Mass/ArcEconomyFragments.h"
#include "Faction/ArcFactionFragments.h"
#include "Mission/ArcMissionSubsystem.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeSubsystem.h"
#include "Strategy/ArcStrategicState.h"
#include "Strategy/ArcStrategyActionSet.h"
#include "ML/ArcStrategyAgentProxy.h"

void UArcFactionStrategySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (MassEntitySubsystem)
    {
        CachedEntityManager = &MassEntitySubsystem->GetMutableEntityManager();
    }

    CachedKnowledgeSub = GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();
    CachedSettlementSub = GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
    CachedMissionSub = GetWorld()->GetSubsystem<UArcMissionSubsystem>();
}

void UArcFactionStrategySubsystem::Deinitialize()
{
    EvaluationQueue.Empty();
    LastDecisionTime.Empty();
    CachedSettlementStates.Empty();
    CachedEntityManager = nullptr;
    CachedKnowledgeSub = nullptr;
    CachedSettlementSub = nullptr;
    CachedMissionSub = nullptr;
    Super::Deinitialize();
}

void UArcFactionStrategySubsystem::Tick(float DeltaTime)
{
    if (!CachedEntityManager || EvaluationQueue.IsEmpty())
    {
        return;
    }

    CachedSettlementStates.Reset();

    const double BudgetSeconds = static_cast<double>(BudgetMs) * 0.001;
    const double StartTime = FPlatformTime::Seconds();
    const double WorldTime = GetWorld()->GetTimeSeconds();
    const int32 QueueSize = EvaluationQueue.Num();
    int32 Processed = 0;

    while (Processed < QueueSize)
    {
        const double Elapsed = FPlatformTime::Seconds() - StartTime;
        if (Elapsed >= BudgetSeconds)
        {
            break;
        }

        if (QueueIndex >= QueueSize)
        {
            QueueIndex = 0;
        }

        const FMassEntityHandle FactionEntity = EvaluationQueue[QueueIndex];
        ++QueueIndex;
        ++Processed;

        if (!CachedEntityManager->IsEntityValid(FactionEntity))
        {
            continue;
        }

        // Check cooldown
        const double* LastTime = LastDecisionTime.Find(FactionEntity);
        if (LastTime && (WorldTime - *LastTime) < static_cast<double>(DecisionCooldownSeconds))
        {
            continue;
        }

        const FMassEntityView FactionView(*CachedEntityManager, FactionEntity);

        const FArcFactionFragment* Faction = FactionView.GetFragmentDataPtr<FArcFactionFragment>();
        const FArcFactionDiplomacyFragment* Diplomacy = FactionView.GetFragmentDataPtr<FArcFactionDiplomacyFragment>();
        const FArcFactionSettlementsFragment* Settlements = FactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();

        if (!Faction || !Diplomacy || !Settlements)
        {
            continue;
        }

        if (!CachedKnowledgeSub || !CachedSettlementSub)
        {
            continue;
        }

        // Build settlement state cache for owned settlements not yet computed this tick
        for (const FMassEntityHandle& SettlementHandle : Settlements->OwnedSettlements)
        {
            if (!CachedEntityManager->IsEntityValid(SettlementHandle))
            {
                continue;
            }

            if (CachedSettlementStates.Contains(SettlementHandle))
            {
                continue;
            }

            const FMassEntityView SettlementView(*CachedEntityManager, SettlementHandle);

            const FArcSettlementFragment* Settlement = SettlementView.GetFragmentDataPtr<FArcSettlementFragment>();
            const FArcSettlementMarketFragment* Market = SettlementView.GetFragmentDataPtr<FArcSettlementMarketFragment>();
            const FArcSettlementWorkforceFragment* Workforce = SettlementView.GetFragmentDataPtr<FArcSettlementWorkforceFragment>();

            if (!Settlement || !Market || !Workforce)
            {
                continue;
            }

            const FArcSettlementStrategicState SettlementState = ArcStrategy::StateComputation::ComputeSettlementState(
                *CachedEntityManager,
                SettlementHandle,
                *Settlement,
                *Market,
                *Workforce,
                *CachedKnowledgeSub,
                *CachedSettlementSub);

            CachedSettlementStates.Add(SettlementHandle, SettlementState);
        }

        const FArcFactionStrategicState FactionState = ArcStrategy::StateComputation::ComputeFactionState(
            *CachedEntityManager,
            FactionEntity,
            *Faction,
            *Diplomacy,
            *Settlements,
            CachedSettlementStates);

        const FArcStrategyDecision Decision = DecideAction(*CachedEntityManager, FactionEntity, FactionState);
        ExecuteDecision(*CachedEntityManager, FactionEntity, Decision);

        LastDecisionTime.FindOrAdd(FactionEntity) = WorldTime;
    }

    CachedSettlementStates.Reset();
}

TStatId UArcFactionStrategySubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UArcFactionStrategySubsystem, STATGROUP_Tickables);
}

void UArcFactionStrategySubsystem::RegisterFaction(FMassEntityHandle FactionEntity)
{
    EvaluationQueue.AddUnique(FactionEntity);
}

void UArcFactionStrategySubsystem::UnregisterFaction(FMassEntityHandle FactionEntity)
{
    const int32 RemovedIndex = EvaluationQueue.Find(FactionEntity);
    if (RemovedIndex != INDEX_NONE)
    {
        EvaluationQueue.RemoveAt(RemovedIndex);
        if (QueueIndex > RemovedIndex && QueueIndex > 0)
        {
            --QueueIndex;
        }
    }
    LastDecisionTime.Remove(FactionEntity);
}

FArcStrategyDecision UArcFactionStrategySubsystem::DecideAction(
    FMassEntityManager& EntityManager,
    FMassEntityHandle FactionEntity,
    const FArcFactionStrategicState& State)
{
    const FMassEntityView FactionView(EntityManager, FactionEntity);
    const FArcFactionFragment* Faction = FactionView.GetFragmentDataPtr<FArcFactionFragment>();
    if (!Faction)
    {
        return FArcStrategyDecision();
    }

    if (Faction->StrategyMode == EArcStrategyDecisionMode::ML && bMLPolicyActive)
    {
        TObjectPtr<UArcStrategyAgentProxy>* ProxyPtr = AgentProxies.Find(FactionEntity);
        if (ProxyPtr && *ProxyPtr)
        {
            UArcStrategyAgentProxy* Proxy = *ProxyPtr;
            Proxy->FactionState = State;
            return Proxy->LastDecision;
        }
    }

    if (Faction->StrategyMode == EArcStrategyDecisionMode::Utility && Faction->FactionActionSet)
    {
        return ScoreUtilityActions(EntityManager, FactionEntity, *Faction->FactionActionSet);
    }

    return FArcStrategyDecision();
}

FArcStrategyDecision UArcFactionStrategySubsystem::ScoreUtilityActions(
    FMassEntityManager& EntityManager,
    FMassEntityHandle FactionEntity,
    const UArcStrategyActionSet& ActionSet)
{
    FArcUtilityContext Context;
    Context.QuerierEntity = FactionEntity;
    Context.EntityManager = &EntityManager;
    Context.World = GetWorld();

    return ArcStrategy::Scoring::EvaluateActionSet(ActionSet, Context);
}

void UArcFactionStrategySubsystem::ExecuteDecision(
    FMassEntityManager& EntityManager,
    FMassEntityHandle FactionEntity,
    const FArcStrategyDecision& Decision)
{
    UE_LOG(LogTemp, Verbose,
        TEXT("UArcFactionStrategySubsystem: Faction [%u:%u] action=%d intensity=%.2f"),
        FactionEntity.Index, FactionEntity.SerialNumber,
        Decision.ActionIndex, Decision.Intensity);
}
