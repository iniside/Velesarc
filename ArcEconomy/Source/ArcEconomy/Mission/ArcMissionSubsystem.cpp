// Copyright Lukasz Baran. All Rights Reserved.

#include "Mission/ArcMissionSubsystem.h"
#include "MassEntitySubsystem.h"
#include "MassCommandBuffer.h"
#include "MassCommands.h"

void UArcMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
    if (MassEntitySubsystem)
    {
        CachedEntityManager = &MassEntitySubsystem->GetMutableEntityManager();
    }
}

void UArcMissionSubsystem::Deinitialize()
{
    ActiveMissions.Empty();
    CachedEntityManager = nullptr;
    Super::Deinitialize();
}

void UArcMissionSubsystem::Tick(float DeltaTime)
{
    // Auto-advance missions from Forming to Rallying when min NPC count reached
    for (TPair<FArcMissionHandle, FArcMission>& Pair : ActiveMissions)
    {
        FArcMission& Mission = Pair.Value;
        if (Mission.Phase == EArcMissionPhase::Forming
            && Mission.AssignedNPCs.Num() >= Mission.MinNPCCount)
        {
            Mission.Phase = EArcMissionPhase::Rallying;
            Mission.CurrentObjective = Mission.TargetLocation;
        }
    }
}

TStatId UArcMissionSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UArcMissionSubsystem, STATGROUP_Tickables);
}

FArcMissionHandle UArcMissionSubsystem::CreateMission(const FArcMission& Template)
{
    FArcMissionHandle Handle = FArcMissionHandle::Make(NextMissionId++);

    FArcMission& NewMission = ActiveMissions.Add(Handle, Template);
    NewMission.Handle = Handle;
    NewMission.Phase = EArcMissionPhase::Forming;

    return Handle;
}

void UArcMissionSubsystem::AssignNPC(FArcMissionHandle MissionHandle, FMassEntityHandle NPCEntity)
{
    FArcMission* Mission = ActiveMissions.Find(MissionHandle);
    if (!Mission || !CachedEntityManager)
    {
        return;
    }

    if (Mission->AssignedNPCs.Num() >= Mission->MaxNPCCount)
    {
        return;
    }

    Mission->AssignedNPCs.Add(NPCEntity);

    // Add assignment fragment to NPC via deferred command
    FMassCommandBuffer& CommandBuffer = CachedEntityManager->Defer();
    FArcMissionAssignmentFragment AssignmentFrag;
    AssignmentFrag.MissionHandle = MissionHandle;
    CommandBuffer.PushCommand<FMassCommandAddFragmentInstances<FArcMissionAssignmentFragment>>(NPCEntity, AssignmentFrag);
}

void UArcMissionSubsystem::UnassignNPC(FArcMissionHandle MissionHandle, FMassEntityHandle NPCEntity)
{
    FArcMission* Mission = ActiveMissions.Find(MissionHandle);
    if (!Mission || !CachedEntityManager)
    {
        return;
    }

    Mission->AssignedNPCs.Remove(NPCEntity);

    FMassCommandBuffer& CommandBuffer = CachedEntityManager->Defer();
    CommandBuffer.RemoveFragment<FArcMissionAssignmentFragment>(NPCEntity);
}

void UArcMissionSubsystem::SetMissionPhase(FArcMissionHandle MissionHandle, EArcMissionPhase NewPhase)
{
    FArcMission* Mission = ActiveMissions.Find(MissionHandle);
    if (Mission)
    {
        Mission->Phase = NewPhase;
    }
}

void UArcMissionSubsystem::CompleteMission(FArcMissionHandle MissionHandle, bool bSuccess)
{
    FArcMission* Mission = ActiveMissions.Find(MissionHandle);
    if (!Mission || !CachedEntityManager)
    {
        return;
    }

    Mission->Phase = bSuccess ? EArcMissionPhase::Completed : EArcMissionPhase::Failed;

    // Remove assignment fragments from all NPCs
    FMassCommandBuffer& CommandBuffer = CachedEntityManager->Defer();
    for (const FMassEntityHandle& NPCHandle : Mission->AssignedNPCs)
    {
        CommandBuffer.RemoveFragment<FArcMissionAssignmentFragment>(NPCHandle);
    }

    ActiveMissions.Remove(MissionHandle);
}

const FArcMission* UArcMissionSubsystem::GetMission(FArcMissionHandle Handle) const
{
    return ActiveMissions.Find(Handle);
}

FArcMission* UArcMissionSubsystem::GetMissionMutable(FArcMissionHandle Handle)
{
    return ActiveMissions.Find(Handle);
}

void UArcMissionSubsystem::GetFactionMissions(FMassEntityHandle FactionEntity, TArray<FArcMissionHandle>& OutHandles) const
{
    for (const TPair<FArcMissionHandle, FArcMission>& Pair : ActiveMissions)
    {
        if (Pair.Value.OwningFaction == FactionEntity)
        {
            OutHandles.Add(Pair.Key);
        }
    }
}
