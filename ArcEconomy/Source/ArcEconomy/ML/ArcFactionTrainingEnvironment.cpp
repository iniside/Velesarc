// Copyright Lukasz Baran. All Rights Reserved.

#include "ML/ArcFactionTrainingEnvironment.h"
#include "ML/ArcArchetypeRewardWeights.h"
#include "ML/ArcStrategyAgentProxy.h"
#include "ML/ArcStrategyTrainingSubsystem.h"
#include "ML/ArcStrategyCurriculumAsset.h"
#include "LearningAgentsManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Faction/ArcFactionFragments.h"

void UArcFactionTrainingEnvironment::GatherAgentReward_Implementation(
    float& OutReward, const int32 AgentId)
{
    OutReward = 0.0f;

    UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));
    if (!Proxy || !ActiveRewardWeights || !CachedEntityManager)
    {
        return;
    }

    const FArcFactionStrategicState& Current = Proxy->FactionState;
    const FArcFactionStrategicState& Previous = Proxy->PreviousFactionState;

    // Delta computation — values are 0–100, normalize deltas to ~[-1, 1]
    const float DeltaWealth = (Current.Wealth - Previous.Wealth) * 0.01f;
    const float DeltaDominance = (Current.Dominance - Previous.Dominance) * 0.01f;
    const float DeltaStability = (Current.Stability - Previous.Stability) * 0.01f;

    // Territory delta — settlement count change
    int32 CurrentSettlementCount = 0;
    if (CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
    {
        const FMassEntityView FactionView(*CachedEntityManager, Proxy->EntityHandle);
        const FArcFactionSettlementsFragment* Settlements = FactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();
        if (Settlements)
        {
            CurrentSettlementCount = Settlements->OwnedSettlements.Num();
        }
    }
    const float DeltaTerritory = static_cast<float>(CurrentSettlementCount - Proxy->PreviousSettlementCount) * 0.1f;

    const float EconomicReward = DeltaWealth;
    const float MilitaryReward = DeltaDominance + DeltaTerritory;
    const float DiplomaticReward = DeltaStability;

    // Survival baseline
    float SurvivalReward = 0.0f;
    EArcFactionArchetype Archetype = EArcFactionArchetype::Economic;

    if (!CachedEntityManager->IsEntityValid(Proxy->EntityHandle) || CurrentSettlementCount == 0)
    {
        const FArcRewardChannelWeights DefaultWeights = ActiveRewardWeights->GetWeights(EArcFactionArchetype::Economic);
        SurvivalReward = -DefaultWeights.SurvivalPenalty;
    }

    // Look up archetype
    if (CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
    {
        const FMassEntityView FactionView(*CachedEntityManager, Proxy->EntityHandle);
        const FArcFactionFragment* Faction = FactionView.GetFragmentDataPtr<FArcFactionFragment>();
        if (Faction)
        {
            Archetype = Faction->Archetype;
        }
    }

    const FArcRewardChannelWeights Weights = ActiveRewardWeights->GetWeights(Archetype);
    OutReward = SurvivalReward
        + (EconomicReward * Weights.Economic)
        + (MilitaryReward * Weights.Military)
        + (DiplomaticReward * Weights.Diplomatic);

    // Cache for next step
    Proxy->PreviousFactionState = Current;
    Proxy->PreviousSettlementCount = CurrentSettlementCount;
}

void UArcFactionTrainingEnvironment::GatherAgentCompletion_Implementation(
    ELearningAgentsCompletion& OutCompletion, const int32 AgentId)
{
    OutCompletion = ELearningAgentsCompletion::Running;

    UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));
    if (!Proxy)
    {
        OutCompletion = ELearningAgentsCompletion::Termination;
        return;
    }

    // Faction collapsed — no settlements remaining
    if (CachedEntityManager && CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
    {
        const FMassEntityView FactionView(*CachedEntityManager, Proxy->EntityHandle);
        const FArcFactionSettlementsFragment* Settlements = FactionView.GetFragmentDataPtr<FArcFactionSettlementsFragment>();
        if (!Settlements || Settlements->OwnedSettlements.IsEmpty())
        {
            OutCompletion = ELearningAgentsCompletion::Termination;
            return;
        }
    }
    else
    {
        OutCompletion = ELearningAgentsCompletion::Termination;
        return;
    }

    // Episode step limit
    if (OwningSubsystem)
    {
        const int32 CurrentStep = OwningSubsystem->GetCurrentEpisodeStep();
        const int32 StageIndex = OwningSubsystem->GetCurrentStageIndex();
        const UArcStrategyCurriculumAsset* Curriculum = OwningSubsystem->GetActiveCurriculum();
        if (Curriculum && StageIndex < Curriculum->Stages.Num())
        {
            if (CurrentStep >= Curriculum->Stages[StageIndex].MaxEpisodeSteps)
            {
                OutCompletion = ELearningAgentsCompletion::Truncation;
            }
        }
    }
}

void UArcFactionTrainingEnvironment::ResetAgentEpisode_Implementation(const int32 AgentId)
{
    UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));
    if (Proxy)
    {
        Proxy->PreviousFactionState = Proxy->FactionState;
        Proxy->PreviousSettlementCount = 0;
    }
}
