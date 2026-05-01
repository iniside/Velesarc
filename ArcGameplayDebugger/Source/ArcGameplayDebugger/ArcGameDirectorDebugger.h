// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

class UArcGameDirectorSubsystem;
struct FMassEntityManager;

class FArcGameDirectorDebugger
{
public:
    void Initialize();
    void Uninitialize();
    void Draw();

    bool bShow = false;

private:
    void DrawRequestQueue();
    void DrawEvaluationHistory();
    void DrawSettlementOverview();

    UArcGameDirectorSubsystem* CachedDirector = nullptr;
    FMassEntityManager* CachedEntityManager = nullptr;

    struct FSettlementOverviewEntry
    {
        FMassEntityHandle Entity;
        FName Name;
        int32 Population = 0;
        int32 MaxPopulation = 0;
        int32 IdleCount = 0;
        int32 WorkerCount = 0;
        int32 TransporterCount = 0;
        int32 GathererCount = 0;
        float FoodPercent = 0.0f;
        float Prosperity = 0.0f;
        bool bHasNPCConfig = false;
    };

    TArray<FSettlementOverviewEntry> CachedOverview;
    float LastOverviewRefreshTime = 0.0f;

    int32 SelectedSettlementIndex = -1;
    FMassEntityHandle FilteredSettlement;
    bool bFilterBySettlement = false;

    // Force spawn controls
    int32 ForceSpawnCount = 5;
};
