// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Mission/ArcMissionTypes.h"
#include "ArcMissionSubsystem.generated.h"

struct FMassEntityManager;

/**
 * Manages mission lifecycle: creation, NPC assignment, phase transitions, cleanup.
 * Missions are data, not Mass entities — same pattern as FArcKnowledgeEntry.
 */
UCLASS()
class ARCECONOMY_API UArcMissionSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    FArcMissionHandle CreateMission(const FArcMission& Template);
    void AssignNPC(FArcMissionHandle MissionHandle, FMassEntityHandle NPCEntity);
    void UnassignNPC(FArcMissionHandle MissionHandle, FMassEntityHandle NPCEntity);
    void SetMissionPhase(FArcMissionHandle MissionHandle, EArcMissionPhase NewPhase);
    void CompleteMission(FArcMissionHandle MissionHandle, bool bSuccess);

    const FArcMission* GetMission(FArcMissionHandle Handle) const;
    FArcMission* GetMissionMutable(FArcMissionHandle Handle);
    void GetFactionMissions(FMassEntityHandle FactionEntity, TArray<FArcMissionHandle>& OutHandles) const;

private:
    TMap<FArcMissionHandle, FArcMission> ActiveMissions;
    uint32 NextMissionId = 1;
    FMassEntityManager* CachedEntityManager = nullptr;
};
