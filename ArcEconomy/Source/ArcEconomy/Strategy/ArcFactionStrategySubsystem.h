// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#include "Strategy/ArcStrategicState.h"
#include "Strategy/ArcStrategyTypes.h"
#include "ArcFactionStrategySubsystem.generated.h"

struct FMassEntityManager;
class UArcKnowledgeSubsystem;
class UArcSettlementSubsystem;
class UArcMissionSubsystem;
class ULearningAgentsNeuralNetwork;
class ULearningAgentsManager;
class ULearningAgentsPolicy;
class UArcFactionStrategyInteractor;
class UArcStrategyAgentProxy;
class UArcStrategyActionSet;

/**
 * Budget-sliced subsystem that evaluates AI faction strategy decisions.
 * Iterates factions in round-robin, computing strategic state and selecting actions.
 * Shares settlement state cache with UArcSettlementStrategySubsystem via getter.
 */
UCLASS()
class ARCECONOMY_API UArcFactionStrategySubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void RegisterFaction(FMassEntityHandle FactionEntity);
    void UnregisterFaction(FMassEntityHandle FactionEntity);

    const TMap<FMassEntityHandle, FArcSettlementStrategicState>& GetSettlementStates() const
    {
        return CachedSettlementStates;
    }

    UPROPERTY(EditAnywhere, Category = "Strategy")
    float BudgetMs = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Strategy")
    float DecisionCooldownSeconds = 15.0f;

    /** Optional trained neural network policy asset. If set, ML decisions replace heuristic. */
    UPROPERTY(EditAnywhere, Category = "ML")
    TObjectPtr<ULearningAgentsNeuralNetwork> PolicyAsset;

    UPROPERTY()
    TObjectPtr<ULearningAgentsManager> MLManager;

    UPROPERTY()
    TObjectPtr<UArcFactionStrategyInteractor> MLInteractor;

    UPROPERTY()
    TObjectPtr<ULearningAgentsPolicy> MLPolicy;

    TMap<FMassEntityHandle, TObjectPtr<UArcStrategyAgentProxy>> AgentProxies;

    bool bMLPolicyActive = false;

protected:
    virtual FArcStrategyDecision DecideAction(
        FMassEntityManager& EntityManager,
        FMassEntityHandle FactionEntity,
        const FArcFactionStrategicState& State);

    FArcStrategyDecision ScoreUtilityActions(
        FMassEntityManager& EntityManager,
        FMassEntityHandle FactionEntity,
        const UArcStrategyActionSet& ActionSet);

    void ExecuteDecision(
        FMassEntityManager& EntityManager,
        FMassEntityHandle FactionEntity,
        const FArcStrategyDecision& Decision);

private:
    TArray<FMassEntityHandle> EvaluationQueue;
    int32 QueueIndex = 0;
    TMap<FMassEntityHandle, double> LastDecisionTime;

    TMap<FMassEntityHandle, FArcSettlementStrategicState> CachedSettlementStates;

    FMassEntityManager* CachedEntityManager = nullptr;
    UArcKnowledgeSubsystem* CachedKnowledgeSub = nullptr;
    UArcSettlementSubsystem* CachedSettlementSub = nullptr;
    UArcMissionSubsystem* CachedMissionSub = nullptr;
};
