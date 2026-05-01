// Copyright Lukasz Baran. All Rights Reserved.

#include "Strategy/ArcSettlementStrategySubsystem.h"
#include "Strategy/ArcStrategyScoring.h"
#include "ML/ArcStrategyAgentProxy.h"
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
#include "Mass/EntityFragments.h"
#include "Director/ArcGameDirectorSubsystem.h"
#include "Strategy/ArcPopulationFragment.h"

void UArcSettlementStrategySubsystem::Initialize(FSubsystemCollectionBase& Collection)
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
    CachedGameDirector = GetWorld()->GetSubsystem<UArcGameDirectorSubsystem>();
}

void UArcSettlementStrategySubsystem::Deinitialize()
{
    EvaluationQueue.Empty();
    LastDecisionTime.Empty();
    CachedEntityManager = nullptr;
    CachedKnowledgeSub = nullptr;
    CachedSettlementSub = nullptr;
    CachedMissionSub = nullptr;
    CachedGameDirector = nullptr;
    Super::Deinitialize();
}

void UArcSettlementStrategySubsystem::Tick(float DeltaTime)
{
    if (!CachedEntityManager || EvaluationQueue.IsEmpty())
    {
        return;
    }

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

        const FMassEntityHandle SettlementEntity = EvaluationQueue[QueueIndex];
        ++QueueIndex;
        ++Processed;

        if (!CachedEntityManager->IsEntityValid(SettlementEntity))
        {
            continue;
        }

        // Check cooldown
        const double* LastTime = LastDecisionTime.Find(SettlementEntity);
        if (LastTime && (WorldTime - *LastTime) < static_cast<double>(DecisionCooldownSeconds))
        {
            continue;
        }

        const FMassEntityView EntityView(*CachedEntityManager, SettlementEntity);

        // Skip if no settlement fragment
        const FArcSettlementFragment* Settlement = EntityView.GetFragmentDataPtr<FArcSettlementFragment>();
        if (!Settlement)
        {
            continue;
        }

        // Skip player-owned settlements
        if (Settlement->bPlayerOwned)
        {
            continue;
        }

        // Skip if no faction link or invalid owning faction
        const FArcSettlementFactionFragment* FactionLink = EntityView.GetFragmentDataPtr<FArcSettlementFactionFragment>();
        if (!FactionLink || !CachedEntityManager->IsEntityValid(FactionLink->OwningFaction))
        {
            continue;
        }

        const FArcSettlementMarketFragment* Market = EntityView.GetFragmentDataPtr<FArcSettlementMarketFragment>();
        const FArcSettlementWorkforceFragment* Workforce = EntityView.GetFragmentDataPtr<FArcSettlementWorkforceFragment>();
        if (!Market || !Workforce)
        {
            continue;
        }

        if (!CachedKnowledgeSub || !CachedSettlementSub)
        {
            continue;
        }

        const FArcSettlementStrategicState State = ArcStrategy::StateComputation::ComputeSettlementState(
            *CachedEntityManager,
            SettlementEntity,
            *Settlement,
            *Market,
            *Workforce,
            *CachedKnowledgeSub,
            *CachedSettlementSub);

        const FArcStrategyDecision Decision = DecideAction(*CachedEntityManager, SettlementEntity, State);
        ExecuteDecision(*CachedEntityManager, SettlementEntity, Decision);

        LastDecisionTime.FindOrAdd(SettlementEntity) = WorldTime;
    }
}

TStatId UArcSettlementStrategySubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UArcSettlementStrategySubsystem, STATGROUP_Tickables);
}

void UArcSettlementStrategySubsystem::RegisterSettlement(FMassEntityHandle SettlementEntity)
{
    EvaluationQueue.AddUnique(SettlementEntity);
}

void UArcSettlementStrategySubsystem::UnregisterSettlement(FMassEntityHandle SettlementEntity)
{
    const int32 RemovedIndex = EvaluationQueue.Find(SettlementEntity);
    if (RemovedIndex != INDEX_NONE)
    {
        EvaluationQueue.RemoveAt(RemovedIndex);
        if (QueueIndex > RemovedIndex && QueueIndex > 0)
        {
            --QueueIndex;
        }
    }
    LastDecisionTime.Remove(SettlementEntity);
}

FArcStrategyDecision UArcSettlementStrategySubsystem::DecideAction(
    FMassEntityManager& EntityManager,
    FMassEntityHandle SettlementEntity,
    const FArcSettlementStrategicState& State)
{
    const FMassEntityView EntityView(EntityManager, SettlementEntity);
    const FArcSettlementFragment* Settlement = EntityView.GetFragmentDataPtr<FArcSettlementFragment>();
    if (!Settlement)
    {
        return FArcStrategyDecision();
    }

    if (Settlement->StrategyMode == EArcStrategyDecisionMode::ML && bMLPolicyActive)
    {
        TObjectPtr<UArcStrategyAgentProxy>* ProxyPtr = AgentProxies.Find(SettlementEntity);
        if (ProxyPtr && *ProxyPtr)
        {
            UArcStrategyAgentProxy* Proxy = *ProxyPtr;
            Proxy->SettlementState = State;
            return Proxy->LastDecision;
        }
    }

    if (Settlement->StrategyMode == EArcStrategyDecisionMode::Utility && Settlement->SettlementActionSet)
    {
        return ScoreUtilityActions(EntityManager, SettlementEntity, *Settlement->SettlementActionSet);
    }

    return FArcStrategyDecision();
}

FArcStrategyDecision UArcSettlementStrategySubsystem::ScoreUtilityActions(
    FMassEntityManager& EntityManager,
    FMassEntityHandle SettlementEntity,
    const UArcStrategyActionSet& ActionSet)
{
    const FMassEntityView EntityView(EntityManager, SettlementEntity);

    FArcUtilityContext Context;
    Context.QuerierEntity = SettlementEntity;
    Context.EntityManager = &EntityManager;
    Context.World = GetWorld();

    const FTransformFragment* TransformFrag = EntityView.GetFragmentDataPtr<FTransformFragment>();
    if (TransformFrag)
    {
        Context.QuerierLocation = TransformFrag->GetTransform().GetLocation();
    }

    return ArcStrategy::Scoring::EvaluateActionSet(ActionSet, Context);
}

void UArcSettlementStrategySubsystem::ExecuteDecision(
    FMassEntityManager& EntityManager,
    FMassEntityHandle SettlementEntity,
    const FArcStrategyDecision& Decision)
{
    const EArcSettlementAction Action = static_cast<EArcSettlementAction>(Decision.ActionIndex);

    switch (Action)
    {
    case EArcSettlementAction::Recruit:
    {
        if (!CachedGameDirector)
        {
            break;
        }
        const FMassEntityView View(EntityManager, SettlementEntity);
        const FArcPopulationFragment* Pop = View.GetFragmentDataPtr<FArcPopulationFragment>();
        if (Pop)
        {
            const int32 Count = FMath::Max(1,
                FMath::RoundToInt32(Decision.Intensity * static_cast<float>(Pop->MaxPopulation)));
            CachedGameDirector->RequestPopulation(SettlementEntity, Count);
        }
        break;
    }
    default:
        UE_LOG(LogTemp, Verbose,
            TEXT("UArcSettlementStrategySubsystem: Settlement [%u:%u] action=%d intensity=%.2f (unhandled)"),
            SettlementEntity.Index, SettlementEntity.SerialNumber,
            Decision.ActionIndex, Decision.Intensity);
        break;
    }
}
