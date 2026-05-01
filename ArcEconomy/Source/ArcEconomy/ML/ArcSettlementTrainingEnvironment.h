// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LearningAgentsTrainingEnvironment.h"
#include "ArcSettlementTrainingEnvironment.generated.h"

class UArcArchetypeRewardWeights;
class UArcStrategyTrainingSubsystem;
struct FMassEntityManager;

/**
 * Training environment for settlement agents. Computes multi-channel rewards
 * (economic + military) shaped by faction archetype weights.
 */
UCLASS()
class ARCECONOMY_API UArcSettlementTrainingEnvironment : public ULearningAgentsTrainingEnvironment
{
    GENERATED_BODY()

public:
    void SetOwningSubsystem(UArcStrategyTrainingSubsystem* InSubsystem) { OwningSubsystem = InSubsystem; }
    void SetRewardWeights(UArcArchetypeRewardWeights* InWeights) { ActiveRewardWeights = InWeights; }
    void SetEntityManager(FMassEntityManager* InEntityManager) { CachedEntityManager = InEntityManager; }

protected:
    virtual void GatherAgentReward_Implementation(float& OutReward, const int32 AgentId) override;
    virtual void GatherAgentCompletion_Implementation(ELearningAgentsCompletion& OutCompletion, const int32 AgentId) override;
    virtual void ResetAgentEpisode_Implementation(const int32 AgentId) override;

private:
    UArcStrategyTrainingSubsystem* OwningSubsystem = nullptr;
    UArcArchetypeRewardWeights* ActiveRewardWeights = nullptr;
    FMassEntityManager* CachedEntityManager = nullptr;
};
