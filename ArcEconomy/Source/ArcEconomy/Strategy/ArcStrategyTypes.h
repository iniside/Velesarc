// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "ArcStrategyTypes.generated.h"

/** Settlement-level strategy actions. */
UENUM(BlueprintType)
enum class EArcSettlementAction : uint8
{
    Build,
    Recruit,
    Trade,
    RequestAid,
    Expand,
    Defend,
    Scout
};

/** Faction-level strategy actions. */
UENUM(BlueprintType)
enum class EArcFactionAction : uint8
{
    DeclareWar,
    MakePeace,
    ProposeAlliance,
    FoundSettlement,
    LaunchAttack,
    EstablishTrade,
    Redistribute
};

/** How a settlement or faction picks its next strategic action. */
UENUM(BlueprintType)
enum class EArcStrategyDecisionMode : uint8
{
    ML,
    Utility
};

/** Result of a strategy decision — action + target + intensity. */
USTRUCT()
struct ARCECONOMY_API FArcStrategyDecision
{
    GENERATED_BODY()

    /** Index into the action enum (cast from EArcSettlementAction or EArcFactionAction). */
    UPROPERTY()
    int32 ActionIndex = 0;

    /** Target entity (neighbor settlement, faction, etc.). Invalid = no target. */
    UPROPERTY()
    FMassEntityHandle TargetEntity;

    /** Target location (for spatial actions like FoundSettlement, Expand). */
    UPROPERTY()
    FVector TargetLocation = FVector::ZeroVector;

    /** Commitment level 0–1. Maps to NPC count, resource amount, etc. */
    UPROPERTY()
    float Intensity = 0.5f;
};
