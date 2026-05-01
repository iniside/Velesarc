// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "ArcGameDirectorTypes.generated.h"

USTRUCT()
struct ARCECONOMY_API FArcPopulationRequest
{
    GENERATED_BODY()

    UPROPERTY()
    FMassEntityHandle SettlementEntity;

    UPROPERTY()
    int32 RequestedCount = 0;

    UPROPERTY()
    double RequestTime = 0.0;
};

USTRUCT()
struct ARCECONOMY_API FArcPopulationEvaluation
{
    GENERATED_BODY()

    // Input
    UPROPERTY()
    FMassEntityHandle SettlementEntity;

    UPROPERTY()
    FName SettlementName;

    UPROPERTY()
    int32 RequestedCount = 0;

    // Factors read from fragments
    UPROPERTY()
    int32 CurrentPopulation = 0;

    UPROPERTY()
    int32 MaxPopulation = 0;

    UPROPERTY()
    int32 IdleCount = 0;

    UPROPERTY()
    int32 CurrentStorage = 0;

    UPROPERTY()
    int32 StorageCap = 0;

    UPROPERTY()
    float Prosperity = 0.0f;

    // Intermediate math
    UPROPERTY()
    int32 CapAdjusted = 0;

    UPROPERTY()
    int32 IdleAdjusted = 0;

    UPROPERTY()
    float HealthMultiplier = 0.0f;

    UPROPERTY()
    int32 GrantedCount = 0;

    // Outcome
    UPROPERTY()
    bool bSpawned = false;

    UPROPERTY()
    double EvalTime = 0.0;
};
