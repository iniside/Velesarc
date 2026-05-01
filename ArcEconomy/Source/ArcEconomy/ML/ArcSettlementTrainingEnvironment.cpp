// Copyright Lukasz Baran. All Rights Reserved.

#include "ML/ArcSettlementTrainingEnvironment.h"
#include "ML/ArcArchetypeRewardWeights.h"
#include "ML/ArcStrategyAgentProxy.h"
#include "ML/ArcStrategyTrainingSubsystem.h"
#include "ML/ArcStrategyCurriculumAsset.h"
#include "LearningAgentsManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "Mass/ArcEconomyFragments.h"
#include "Faction/ArcFactionFragments.h"
#include "Strategy/ArcPopulationFragment.h"

void UArcSettlementTrainingEnvironment::GatherAgentReward_Implementation(
    float& OutReward, const int32 AgentId)
{
    OutReward = 0.0f;

    UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));
    if (!Proxy || !ActiveRewardWeights || !CachedEntityManager)
    {
        return;
    }

    const FArcSettlementStrategicState& Current = Proxy->SettlementState;
    const FArcSettlementStrategicState& Previous = Proxy->PreviousSettlementState;

    // Delta computation — values are 0–100, normalize deltas to ~[-1, 1]
    const float DeltaProsperity = (Current.Prosperity - Previous.Prosperity) * 0.01f;
    const float DeltaSecurity = (Current.Security - Previous.Security) * 0.01f;

    const float EconomicReward = DeltaProsperity;
    const float MilitaryReward = DeltaSecurity;

    // Survival baseline
    float SurvivalReward = 0.0f;
    if (!CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
    {
        const FArcRewardChannelWeights Weights = ActiveRewardWeights->GetWeights(EArcFactionArchetype::Economic);
        SurvivalReward = -Weights.SurvivalPenalty;
    }
    else
    {
        const FMassEntityView EntityView(*CachedEntityManager, Proxy->EntityHandle);
        const FArcPopulationFragment* Pop = EntityView.GetFragmentDataPtr<FArcPopulationFragment>();
        if (Pop && Pop->Population <= 0)
        {
            const FArcRewardChannelWeights Weights = ActiveRewardWeights->GetWeights(EArcFactionArchetype::Economic);
            SurvivalReward = -Weights.SurvivalPenalty;
        }
    }

    // Look up owning faction archetype for reward weighting
    EArcFactionArchetype Archetype = EArcFactionArchetype::Economic;
    if (CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
    {
        const FMassEntityView SettlementView(*CachedEntityManager, Proxy->EntityHandle);
        const FArcSettlementFactionFragment* FactionLink = SettlementView.GetFragmentDataPtr<FArcSettlementFactionFragment>();
        if (FactionLink && CachedEntityManager->IsEntityValid(FactionLink->OwningFaction))
        {
            const FMassEntityView FactionView(*CachedEntityManager, FactionLink->OwningFaction);
            const FArcFactionFragment* Faction = FactionView.GetFragmentDataPtr<FArcFactionFragment>();
            if (Faction)
            {
                Archetype = Faction->Archetype;
            }
        }
    }

    const FArcRewardChannelWeights Weights = ActiveRewardWeights->GetWeights(Archetype);
    OutReward = SurvivalReward
        + (EconomicReward * Weights.Economic)
        + (MilitaryReward * Weights.Military);

    // Cache current as previous for next step
    Proxy->PreviousSettlementState = Current;
    Proxy->DefenseEventCount = 0;
}

void UArcSettlementTrainingEnvironment::GatherAgentCompletion_Implementation(
    ELearningAgentsCompletion& OutCompletion, const int32 AgentId)
{
    OutCompletion = ELearningAgentsCompletion::Running;

    UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));
    if (!Proxy)
    {
        OutCompletion = ELearningAgentsCompletion::Termination;
        return;
    }

    // Check entity validity (settlement destroyed)
    if (!CachedEntityManager || !CachedEntityManager->IsEntityValid(Proxy->EntityHandle))
    {
        OutCompletion = ELearningAgentsCompletion::Termination;
        return;
    }

    // Check population collapse
    const FMassEntityView EntityView(*CachedEntityManager, Proxy->EntityHandle);
    const FArcPopulationFragment* Pop = EntityView.GetFragmentDataPtr<FArcPopulationFragment>();
    if (Pop && Pop->Population <= 0)
    {
        OutCompletion = ELearningAgentsCompletion::Termination;
        return;
    }

    // Check episode step limit from curriculum
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

void UArcSettlementTrainingEnvironment::ResetAgentEpisode_Implementation(const int32 AgentId)
{
    UArcStrategyAgentProxy* Proxy = Cast<UArcStrategyAgentProxy>(Manager->GetAgent(AgentId));
    if (Proxy)
    {
        Proxy->PreviousSettlementState = Proxy->SettlementState;
        Proxy->DefenseEventCount = 0;
    }
}
