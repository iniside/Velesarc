// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Strategy/ArcStrategicState.h"
#include "Strategy/ArcStrategyTypes.h"
#include "ArcSettlementStrategySubsystem.generated.h"

struct FMassEntityManager;
class UArcStrategyActionSet;
class UArcKnowledgeSubsystem;
class UArcSettlementSubsystem;
class UArcMissionSubsystem;
class UArcGameDirectorSubsystem;
class ULearningAgentsNeuralNetwork;
class ULearningAgentsManager;
class ULearningAgentsPolicy;
class UArcSettlementStrategyInteractor;
class UArcStrategyAgentProxy;

/**
 * Budget-sliced subsystem that evaluates AI settlement strategy decisions.
 * Iterates settlements in round-robin, computing strategic state and selecting actions.
 *
 * Decision method is pluggable: defaults to rule-based heuristic,
 * can be replaced by ML policy (see Plan B).
 */
UCLASS()
class ARCECONOMY_API UArcSettlementStrategySubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void RegisterSettlement(FMassEntityHandle SettlementEntity);
    void UnregisterSettlement(FMassEntityHandle SettlementEntity);

    UPROPERTY(EditAnywhere, Category = "Strategy")
    float BudgetMs = 2.0f;

    UPROPERTY(EditAnywhere, Category = "Strategy")
    float DecisionCooldownSeconds = 5.0f;

    /** Optional trained neural network policy asset. If set, ML decisions replace heuristic. */
    UPROPERTY(EditAnywhere, Category = "ML")
    TObjectPtr<ULearningAgentsNeuralNetwork> PolicyAsset;

    UPROPERTY()
    TObjectPtr<ULearningAgentsManager> MLManager;

    UPROPERTY()
    TObjectPtr<UArcSettlementStrategyInteractor> MLInteractor;

    UPROPERTY()
    TObjectPtr<ULearningAgentsPolicy> MLPolicy;

    TMap<FMassEntityHandle, TObjectPtr<UArcStrategyAgentProxy>> AgentProxies;

    bool bMLPolicyActive = false;

protected:
    virtual FArcStrategyDecision DecideAction(
        FMassEntityManager& EntityManager,
        FMassEntityHandle SettlementEntity,
        const FArcSettlementStrategicState& State);

    FArcStrategyDecision ScoreUtilityActions(
        FMassEntityManager& EntityManager,
        FMassEntityHandle SettlementEntity,
        const UArcStrategyActionSet& ActionSet);

    void ExecuteDecision(
        FMassEntityManager& EntityManager,
        FMassEntityHandle SettlementEntity,
        const FArcStrategyDecision& Decision);

private:
    TArray<FMassEntityHandle> EvaluationQueue;
    int32 QueueIndex = 0;
    TMap<FMassEntityHandle, double> LastDecisionTime;

    FMassEntityManager* CachedEntityManager = nullptr;
    UArcKnowledgeSubsystem* CachedKnowledgeSub = nullptr;
    UArcSettlementSubsystem* CachedSettlementSub = nullptr;
    UArcMissionSubsystem* CachedMissionSub = nullptr;
    UArcGameDirectorSubsystem* CachedGameDirector = nullptr;
};
