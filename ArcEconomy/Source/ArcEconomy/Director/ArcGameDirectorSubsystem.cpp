// Copyright Lukasz Baran. All Rights Reserved.

#include "Director/ArcGameDirectorSubsystem.h"

#include "HAL/PlatformTime.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassEntityConfigAsset.h"
#include "MassSpawnerSubsystem.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/EntityFragments.h"
#include "Strategy/ArcPopulationFragment.h"
#include "Strategy/ArcStrategicState.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeSubsystem.h"
#include "Engine/World.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSQueryInstance.h"
#include "ArcTQSQuerySubsystem.h"
#include "ArcTQSTypes.h"
#include "ArcMass/ArcMassFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcGameDirectorSubsystem)

// ============================================================================
// Initialize / Deinitialize / Tick
// ============================================================================

void UArcGameDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UMassEntitySubsystem* EntitySubsystem = Collection.InitializeDependency<UMassEntitySubsystem>();
    if (EntitySubsystem)
    {
        CachedEntityManager = &EntitySubsystem->GetMutableEntityManager();
    }

    CachedTQSSubsystem = GetWorld()->GetSubsystem<UArcTQSQuerySubsystem>();
}

void UArcGameDirectorSubsystem::Deinitialize()
{
    if (CachedTQSSubsystem)
    {
        for (const FArcPendingSpawn& Pending : PendingSpawns)
        {
            if (Pending.TQSQueryId != INDEX_NONE)
            {
                CachedTQSSubsystem->AbortQuery(Pending.TQSQueryId);
            }
        }
    }

    PendingSpawns.Empty();
    InFlightSettlements.Empty();
    RequestQueue.Empty();
    EvaluationHistory.Empty();
    CachedEntityManager = nullptr;
    CachedTQSSubsystem = nullptr;

    Super::Deinitialize();
}

void UArcGameDirectorSubsystem::Tick(float DeltaTime)
{
    if (!CachedEntityManager || RequestQueue.IsEmpty())
    {
        return;
    }

    const double BudgetSeconds = static_cast<double>(BudgetMs) * 0.001;
    const double StartTime = FPlatformTime::Seconds();

    int32 Index = 0;
    while (Index < RequestQueue.Num())
    {
        if (FPlatformTime::Seconds() - StartTime >= BudgetSeconds)
        {
            break;
        }

        const FArcPopulationRequest& Request = RequestQueue[Index];

        // Skip settlements with in-flight TQS queries
        if (InFlightSettlements.Contains(Request.SettlementEntity))
        {
            ++Index;
            continue;
        }

        // Dequeue this request
        const FArcPopulationRequest DequeuedRequest = Request;
        RequestQueue.RemoveAt(Index, EAllowShrinking::No);
        // Don't increment Index — next element shifted into current slot

        // Validate settlement entity still valid
        if (!CachedEntityManager->IsEntityValid(DequeuedRequest.SettlementEntity))
        {
            continue;
        }

        FArcPopulationEvaluation Evaluation = EvaluateRequest(*CachedEntityManager, DequeuedRequest);

        if (Evaluation.GrantedCount > 0)
        {
            BeginSpawn(*CachedEntityManager, DequeuedRequest.SettlementEntity, Evaluation.GrantedCount);
        }

        // Ring buffer: cap history at MaxHistorySize
        if (EvaluationHistory.Num() >= MaxHistorySize)
        {
            EvaluationHistory.RemoveAt(0, EAllowShrinking::No);
        }
        EvaluationHistory.Add(Evaluation);
    }
}

TStatId UArcGameDirectorSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UArcGameDirectorSubsystem, STATGROUP_Tickables);
}

// ============================================================================
// RequestPopulation
// ============================================================================

void UArcGameDirectorSubsystem::RequestPopulation(FMassEntityHandle SettlementEntity, int32 Count)
{
    if (Count <= 0)
    {
        return;
    }

    FArcPopulationRequest Request;
    Request.SettlementEntity = SettlementEntity;
    Request.RequestedCount = Count;
    Request.RequestTime = FPlatformTime::Seconds();
    RequestQueue.Add(Request);
}

// ============================================================================
// EvaluateRequest
// ============================================================================

FArcPopulationEvaluation UArcGameDirectorSubsystem::EvaluateRequest(
    FMassEntityManager& EntityManager,
    const FArcPopulationRequest& Request)
{
    FArcPopulationEvaluation Eval;
    Eval.SettlementEntity = Request.SettlementEntity;
    Eval.RequestedCount = Request.RequestedCount;
    Eval.EvalTime = FPlatformTime::Seconds();

    const FMassEntityView EntityView(EntityManager, Request.SettlementEntity);

    // Read settlement fragments
    const FArcSettlementFragment* Settlement = EntityView.GetFragmentDataPtr<FArcSettlementFragment>();
    const FArcPopulationFragment* Population = EntityView.GetFragmentDataPtr<FArcPopulationFragment>();
    const FArcSettlementMarketFragment* Market = EntityView.GetFragmentDataPtr<FArcSettlementMarketFragment>();
    const FArcSettlementWorkforceFragment* Workforce = EntityView.GetFragmentDataPtr<FArcSettlementWorkforceFragment>();

    if (!Settlement || !Population || !Market || !Workforce)
    {
        Eval.GrantedCount = 0;
        return Eval;
    }

    // Capture fragment values
    Eval.SettlementName = Settlement->SettlementName;
    Eval.CurrentPopulation = Population->Population;
    Eval.MaxPopulation = Population->MaxPopulation;
    Eval.IdleCount = Workforce->IdleCount;
    Eval.CurrentStorage = Market->CurrentTotalStorage;
    Eval.StorageCap = Market->TotalStorageCap;

    // Compute prosperity via strategic state
    UWorld* World = GetWorld();
    UArcKnowledgeSubsystem* KnowledgeSub = World ? World->GetSubsystem<UArcKnowledgeSubsystem>() : nullptr;
    UArcSettlementSubsystem* SettlementSub = World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;

    if (KnowledgeSub && SettlementSub)
    {
        const FArcSettlementStrategicState StrategicState = ArcStrategy::StateComputation::ComputeSettlementState(
            EntityManager,
            Request.SettlementEntity,
            *Settlement,
            *Market,
            *Workforce,
            *KnowledgeSub,
            *SettlementSub);
        Eval.Prosperity = StrategicState.Prosperity;
    }

    // --- Grant computation ---

    // Housing cap
    int32 Grant = FMath::Min(Request.RequestedCount, Eval.MaxPopulation - Eval.CurrentPopulation);
    Eval.CapAdjusted = Grant;

    // Zero-population bootstrap: if settlement has no NPCs at all, skip health
    // gating and grant the full cap-adjusted amount so the economy can start.
    if (Eval.CurrentPopulation == 0)
    {
        Eval.IdleAdjusted = Grant;
        Eval.HealthMultiplier = 1.0f;
        Eval.GrantedCount = FMath::Max(Grant, 0);
        return Eval;
    }

    // Subtract already-idle workers (spare hands)
    Grant = Grant - Eval.IdleCount;
    Eval.IdleAdjusted = Grant;

    // Health multiplier from food ratio and prosperity
    const float FoodRatio = (Eval.StorageCap > 0)
        ? static_cast<float>(Eval.CurrentStorage) / static_cast<float>(Eval.StorageCap)
        : 0.0f;
    const float ProsperityFactor = Eval.Prosperity * ProsperityWeight;
    const float HealthMul = FMath::Clamp(FoodRatio * ProsperityFactor, MinHealthMultiplier, 1.0f);
    Eval.HealthMultiplier = HealthMul;

    Grant = FMath::RoundToInt(static_cast<float>(Grant) * HealthMul);
    Grant = FMath::Max(Grant, 0);
    Eval.GrantedCount = Grant;

    return Eval;
}

// ============================================================================
// BeginSpawn
// ============================================================================

void UArcGameDirectorSubsystem::BeginSpawn(
    FMassEntityManager& EntityManager,
    FMassEntityHandle SettlementEntity,
    int32 Count)
{
    if (!CachedTQSSubsystem)
    {
        UE_LOG(LogTemp, Warning, TEXT("UArcGameDirectorSubsystem: TQS subsystem not available."));
        return;
    }

    const FMassEntityView EntityView(EntityManager, SettlementEntity);

    const FArcSettlementFragment* Settlement = EntityView.GetFragmentDataPtr<FArcSettlementFragment>();
    if (!Settlement || !Settlement->NPCEntityConfig)
    {
        return;
    }

    if (!Settlement->SpawnLocationQuery)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UArcGameDirectorSubsystem: Settlement '%s' has no SpawnLocationQuery configured."),
            *Settlement->SettlementName.ToString());
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FMassEntityTemplate& EntityTemplate = Settlement->NPCEntityConfig->GetOrCreateEntityTemplate(*World);
    if (!EntityTemplate.IsValid())
    {
        return;
    }

    // Build TQS query context
    FArcTQSQueryContext QueryContext;
    QueryContext.QuerierEntity = SettlementEntity;
    QueryContext.World = World;
    QueryContext.EntityManager = CachedEntityManager;

    const FTransformFragment* TransformFrag = EntityView.GetFragmentDataPtr<FTransformFragment>();
    if (TransformFrag)
    {
        QueryContext.QuerierLocation = TransformFrag->GetTransform().GetLocation();
    }

    // Create pending spawn record
    FArcPendingSpawn Pending;
    Pending.SettlementEntity = SettlementEntity;
    Pending.Count = Count;
    Pending.TemplateID = EntityTemplate.GetTemplateID();

    // Mark settlement as in-flight
    InFlightSettlements.Add(SettlementEntity);

    // Fire async TQS query
    const int32 QueryId = CachedTQSSubsystem->RunQuery(
        Settlement->SpawnLocationQuery,
        QueryContext,
        FArcTQSQueryFinished::CreateUObject(this, &UArcGameDirectorSubsystem::OnSpawnQueryCompleted));

    if (QueryId == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UArcGameDirectorSubsystem: TQS query failed to start for settlement '%s'."),
            *Settlement->SettlementName.ToString());
        InFlightSettlements.Remove(SettlementEntity);
        return;
    }

    Pending.TQSQueryId = QueryId;
    PendingSpawns.Add(Pending);
}

// ============================================================================
// OnSpawnQueryCompleted
// ============================================================================

void UArcGameDirectorSubsystem::OnSpawnQueryCompleted(FArcTQSQueryInstance& CompletedQuery)
{
    // Find matching pending spawn by query ID
    const int32 PendingIndex = PendingSpawns.IndexOfByPredicate(
        [&CompletedQuery](const FArcPendingSpawn& Pending)
        {
            return Pending.TQSQueryId == CompletedQuery.QueryId;
        });

    if (PendingIndex == INDEX_NONE)
    {
        return;
    }

    const FArcPendingSpawn Pending = PendingSpawns[PendingIndex];
    PendingSpawns.RemoveAtSwap(PendingIndex, EAllowShrinking::No);
    InFlightSettlements.Remove(Pending.SettlementEntity);

    // Validate settlement still alive
    if (!CachedEntityManager || !CachedEntityManager->IsEntityValid(Pending.SettlementEntity))
    {
        return;
    }

    // Check query results
    if (CompletedQuery.Status != EArcTQSQueryStatus::Completed || CompletedQuery.Results.IsEmpty())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UArcGameDirectorSubsystem: TQS query returned no results for spawn."));
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UMassSpawnerSubsystem* SpawnerSystem = World->GetSubsystem<UMassSpawnerSubsystem>();
    if (!SpawnerSystem)
    {
        return;
    }

    // Clamp to available locations
    const int32 ResultCount = CompletedQuery.Results.Num();
    const int32 ActualCount = FMath::Min(Pending.Count, ResultCount);
    if (ActualCount < Pending.Count)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UArcGameDirectorSubsystem: TQS returned %d locations but %d requested — clamping."),
            ResultCount, Pending.Count);
    }

    // Select spatially distributed locations via greedy farthest-point picking.
    // First point is index 0, each subsequent pick maximizes minimum distance to
    // all already-selected points. This prevents NPCs stacking on top of each other.
    TArray<int32> SelectedIndices;
    SelectedIndices.Reserve(ActualCount);

    if (ActualCount > 0)
    {
        SelectedIndices.Add(0);

        // Track squared distance from each candidate to the nearest selected point
        TArray<float> MinDistSq;
        MinDistSq.SetNumUninitialized(ResultCount);
        const FVector& FirstLocation = CompletedQuery.Results[0].Location;
        for (int32 i = 0; i < ResultCount; ++i)
        {
            MinDistSq[i] = FVector::DistSquared(CompletedQuery.Results[i].Location, FirstLocation);
        }
        MinDistSq[0] = -1.0f; // First point already selected

        for (int32 Pick = 1; Pick < ActualCount; ++Pick)
        {
            // Find candidate with largest minimum distance to selected set
            int32 BestIndex = INDEX_NONE;
            float BestDistSq = -1.0f;
            for (int32 i = 0; i < ResultCount; ++i)
            {
                if (MinDistSq[i] > BestDistSq)
                {
                    BestDistSq = MinDistSq[i];
                    BestIndex = i;
                }
            }

            if (BestIndex == INDEX_NONE)
            {
                break;
            }

            SelectedIndices.Add(BestIndex);

            // Update min distances: for each candidate, shrink if new pick is closer
            const FVector& NewLocation = CompletedQuery.Results[BestIndex].Location;
            for (int32 i = 0; i < ResultCount; ++i)
            {
                const float DistToNew = FVector::DistSquared(CompletedQuery.Results[i].Location, NewLocation);
                MinDistSq[i] = FMath::Min(MinDistSq[i], DistToNew);
            }
            // Zero out selected point so it can't be picked again
            MinDistSq[BestIndex] = -1.0f;
        }
    }

    // Spawn entities
    const int32 SpawnCount = SelectedIndices.Num();
    TArray<FMassEntityHandle> SpawnedEntities;
    TSharedPtr<FMassEntityManager::FEntityCreationContext> CreationContext =
        SpawnerSystem->SpawnEntities(Pending.TemplateID, SpawnCount,
            FConstStructView(), nullptr, SpawnedEntities);

    // Set transforms from selected TQS results
    const int32 SpawnedCount = SpawnedEntities.Num();
    for (int32 i = 0; i < SpawnedCount; ++i)
    {
        FTransformFragment* SpawnedTransform =
            CachedEntityManager->GetFragmentDataPtr<FTransformFragment>(SpawnedEntities[i]);
        if (SpawnedTransform)
        {
            const int32 ResultIndex = SelectedIndices[i];
            FVector SpawnLocation = CompletedQuery.Results[ResultIndex].Location;

            const FArcAgentCapsuleFragment* CapsuleFrag =
                CachedEntityManager->GetFragmentDataPtr<FArcAgentCapsuleFragment>(SpawnedEntities[i]);
            if (CapsuleFrag)
            {
                SpawnLocation.Z += CapsuleFrag->HalfHeight;
            }

            SpawnedTransform->GetMutableTransform().SetLocation(SpawnLocation);
        }
    }

    // Release CreationContext — deferred commands flush, observers fire
    CreationContext.Reset();

    // Update evaluation history: mark the most recent eval for this settlement as spawned
    for (int32 i = EvaluationHistory.Num() - 1; i >= 0; --i)
    {
        if (EvaluationHistory[i].SettlementEntity == Pending.SettlementEntity && !EvaluationHistory[i].bSpawned)
        {
            EvaluationHistory[i].bSpawned = true;
            break;
        }
    }
}
