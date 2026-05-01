// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassEntityTypes.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "ArcMissionTypes.generated.h"

/** Lightweight handle to a mission, same pattern as FArcKnowledgeHandle. */
USTRUCT(BlueprintType)
struct ARCECONOMY_API FArcMissionHandle
{
    GENERATED_BODY()

    FArcMissionHandle() = default;
    explicit FArcMissionHandle(uint32 InValue) : Value(InValue) {}

    bool IsValid() const { return Value != 0; }
    uint32 GetValue() const { return Value; }

    bool operator==(const FArcMissionHandle& Other) const { return Value == Other.Value; }
    bool operator!=(const FArcMissionHandle& Other) const { return Value != Other.Value; }

    friend uint32 GetTypeHash(const FArcMissionHandle& Handle) { return ::GetTypeHash(Handle.Value); }

    static FArcMissionHandle Make(uint32 InValue) { return FArcMissionHandle(InValue); }

private:
    UPROPERTY()
    uint32 Value = 0;
};

UENUM(BlueprintType)
enum class EArcMissionPhase : uint8
{
    Forming,     // Waiting for minimum NPCs to be assigned
    Rallying,    // NPCs moving to rally point
    Executing,   // Mission in progress
    Completed,   // Success
    Failed       // Aborted or destroyed
};

/**
 * A coordinated multi-NPC action managed by UArcMissionSubsystem.
 */
USTRUCT()
struct ARCECONOMY_API FArcMission
{
    GENERATED_BODY()

    FArcMissionHandle Handle;

    UPROPERTY()
    FGameplayTag GoalTag;

    UPROPERTY()
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY()
    FMassEntityHandle TargetEntity;

    UPROPERTY()
    FMassEntityHandle OwningSettlement;

    UPROPERTY()
    FMassEntityHandle OwningFaction;

    UPROPERTY()
    TArray<FMassEntityHandle> AssignedNPCs;

    UPROPERTY()
    EArcMissionPhase Phase = EArcMissionPhase::Forming;

    UPROPERTY()
    int32 MinNPCCount = 1;

    UPROPERTY()
    int32 MaxNPCCount = 10;

    UPROPERTY()
    FStateTreeReference MissionStateTree;

    /** Current objective location — written by StateTree, read by assigned NPCs. */
    UPROPERTY()
    FVector CurrentObjective = FVector::ZeroVector;
};

/**
 * Fragment placed on NPCs assigned to a mission.
 * Their individual StateTree reads this to switch to mission behavior.
 */
USTRUCT()
struct ARCECONOMY_API FArcMissionAssignmentFragment : public FMassFragment
{
    GENERATED_BODY()

    UPROPERTY()
    FArcMissionHandle MissionHandle;
};
