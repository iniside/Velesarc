// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Director/ArcGameDirectorTypes.h"
#include "MassEntityTemplate.h"
#include "ArcGameDirectorSubsystem.generated.h"

struct FMassEntityManager;
struct FArcTQSQueryInstance;
class UArcTQSQuerySubsystem;

UCLASS()
class ARCECONOMY_API UArcGameDirectorSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    void RequestPopulation(FMassEntityHandle SettlementEntity, int32 Count);

    const TArray<FArcPopulationRequest>& GetPendingRequests() const { return RequestQueue; }
    const TArray<FArcPopulationEvaluation>& GetEvaluationHistory() const { return EvaluationHistory; }

    UPROPERTY(EditAnywhere, Category = "Director")
    float BudgetMs = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Director|Population")
    float MinHealthMultiplier = 0.25f;

    UPROPERTY(EditAnywhere, Category = "Director|Population")
    float ProsperityWeight = 0.01f;

private:
    FArcPopulationEvaluation EvaluateRequest(
        FMassEntityManager& EntityManager,
        const FArcPopulationRequest& Request);

    void BeginSpawn(
        FMassEntityManager& EntityManager,
        FMassEntityHandle SettlementEntity,
        int32 Count);

    void OnSpawnQueryCompleted(FArcTQSQueryInstance& CompletedQuery);

    struct FArcPendingSpawn
    {
        FMassEntityHandle SettlementEntity;
        int32 Count = 0;
        FMassEntityTemplateID TemplateID;
        int32 TQSQueryId = INDEX_NONE;
    };

    TArray<FArcPopulationRequest> RequestQueue;
    TArray<FArcPopulationEvaluation> EvaluationHistory;
    static constexpr int32 MaxHistorySize = 50;

    TArray<FArcPendingSpawn> PendingSpawns;
    TSet<FMassEntityHandle> InFlightSettlements;

    FMassEntityManager* CachedEntityManager = nullptr;
    UArcTQSQuerySubsystem* CachedTQSSubsystem = nullptr;
};
