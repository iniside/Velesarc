// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mass/EntityHandle.h"
#if WITH_EDITOR
#include "LearningAgentsCommunicator.h"
#endif
#include "ArcStrategyTrainingSubsystem.generated.h"

class ULearningAgentsManager;
class ULearningAgentsPolicy;
class ULearningAgentsCritic;
class ULearningAgentsPPOTrainer;
class UArcSettlementStrategyInteractor;
class UArcFactionStrategyInteractor;
class UArcSettlementTrainingEnvironment;
class UArcFactionTrainingEnvironment;
class UArcArchetypeRewardWeights;
class UArcStrategyCurriculumAsset;
class UArcStrategyAgentProxy;
class AArcStrategyTrainingCoordinator;
struct FMassEntityManager;

/** Telemetry snapshot for dashboard consumption. */
USTRUCT(BlueprintType)
struct FArcTrainingTelemetry
{
    GENERATED_BODY()

    float SettlementEconomicMean = 0.0f;
    float SettlementMilitaryMean = 0.0f;
    float FactionEconomicMean = 0.0f;
    float FactionMilitaryMean = 0.0f;
    float FactionDiplomaticMean = 0.0f;
};

/**
 * Editor-only world subsystem that orchestrates PPO training for settlement
 * and faction strategy agents via the Learning Agents plugin.
 */
UCLASS()
class ARCECONOMY_API UArcStrategyTrainingSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableInEditor() const override { return true; }

    // ----- Training Control -----

    UFUNCTION(BlueprintCallable, Category = "Training")
    void StartTraining(UArcStrategyCurriculumAsset* Curriculum, UArcArchetypeRewardWeights* RewardWeights);

    UFUNCTION(BlueprintCallable, Category = "Training")
    void StopTraining();

    UFUNCTION(BlueprintCallable, Category = "Training")
    void SavePoliciesNow();

    UFUNCTION(BlueprintPure, Category = "Training")
    bool IsTraining() const { return bIsTraining; }

    // ----- Telemetry Getters (for dashboard) -----

    UFUNCTION(BlueprintPure, Category = "Training")
    int32 GetCurrentStageIndex() const { return CurrentStageIndex; }

    UFUNCTION(BlueprintPure, Category = "Training")
    int32 GetCurrentEpisodeStep() const { return CurrentEpisodeStep; }

    UFUNCTION(BlueprintPure, Category = "Training")
    int32 GetTotalEpisodes() const { return TotalEpisodes; }

    const UArcStrategyCurriculumAsset* GetActiveCurriculum() const { return ActiveCurriculum; }

    const TArray<float>& GetSettlementRewardHistory() const { return SettlementRewardHistory; }
    const TArray<float>& GetFactionRewardHistory() const { return FactionRewardHistory; }
    FArcTrainingTelemetry GetPerChannelRewards() const { return ChannelTelemetry; }

    // ----- Settings (configurable from dashboard) -----

    UPROPERTY(EditAnywhere, Category = "Training")
    FString SavePathPrefix = TEXT("/Game/ML/Strategy/");

    /** Action bitmask derived from current curriculum stage. Bit N set = action N allowed. */
    uint32 SettlementActionMask = 0xFFFFFFFF;
    uint32 FactionActionMask = 0xFFFFFFFF;

private:
    // ----- Pipeline Setup -----
    void SetupWorld(const struct FArcCurriculumStage& Stage);
    void TeardownWorld();
    void SetupLearningAgents();
    void TeardownLearningAgents();
    void UpdateProxyStates();
    void CheckCurriculumAdvancement();
    void RecordTelemetry();
    void SimulateSettlementActions();
    void SimulateFactionActions();

    // ----- State -----
    bool bIsTraining = false;
    bool bFirstTick = true;
    int32 CurrentStageIndex = 0;
    int32 CurrentEpisodeStep = 0;
    int32 ConsecutiveGoodEpisodes = 0;
    float AccumulatedSettlementReward = 0.0f;
    float AccumulatedFactionReward = 0.0f;
    int32 TotalEpisodes = 0;

    // ----- Telemetry Ring Buffers -----
    static constexpr int32 MaxRewardHistory = 500;
    TArray<float> SettlementRewardHistory;
    TArray<float> FactionRewardHistory;
    FArcTrainingTelemetry ChannelTelemetry;

    // ----- Data Assets -----

    UPROPERTY()
    TObjectPtr<UArcStrategyCurriculumAsset> ActiveCurriculum;

    UPROPERTY()
    TObjectPtr<UArcArchetypeRewardWeights> ActiveRewardWeights;

    // ----- Settlement Pipeline -----

    UPROPERTY()
    TObjectPtr<AArcStrategyTrainingCoordinator> Coordinator;

    UPROPERTY()
    TObjectPtr<UArcSettlementStrategyInteractor> SettlementInteractor;

    UPROPERTY()
    TObjectPtr<UArcSettlementTrainingEnvironment> SettlementEnvironment;

    UPROPERTY()
    TObjectPtr<ULearningAgentsPolicy> SettlementPolicy;

    UPROPERTY()
    TObjectPtr<ULearningAgentsCritic> SettlementCritic;

    UPROPERTY()
    TObjectPtr<ULearningAgentsPPOTrainer> SettlementTrainer;

#if WITH_EDITORONLY_DATA
    FLearningAgentsCommunicator SettlementCommunicator;
#endif

    // ----- Faction Pipeline -----

    UPROPERTY()
    TObjectPtr<UArcFactionStrategyInteractor> FactionInteractor;

    UPROPERTY()
    TObjectPtr<UArcFactionTrainingEnvironment> FactionEnvironment;

    UPROPERTY()
    TObjectPtr<ULearningAgentsPolicy> FactionPolicy;

    UPROPERTY()
    TObjectPtr<ULearningAgentsCritic> FactionCritic;

    UPROPERTY()
    TObjectPtr<ULearningAgentsPPOTrainer> FactionTrainer;

#if WITH_EDITORONLY_DATA
    FLearningAgentsCommunicator FactionCommunicator;
#endif

    // ----- Agent Proxies -----

    UPROPERTY()
    TArray<TObjectPtr<UArcStrategyAgentProxy>> SettlementProxies;

    UPROPERTY()
    TArray<TObjectPtr<UArcStrategyAgentProxy>> FactionProxies;

    // ----- Spawned Training Entities -----
    TArray<FMassEntityHandle> SpawnedSettlements;
    TArray<FMassEntityHandle> SpawnedFactions;

    FMassEntityManager* CachedEntityManager = nullptr;
};
