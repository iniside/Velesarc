// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcGovernorAssignWorkersTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcEconomyTypes.h"
#include "ArcSettlementSubsystem.h"
#include "MassStateTreeExecutionContext.h"
#include "MassCommonFragments.h"
#include "StateTreeLinker.h"
#include "ArcAreaSubsystem.h"
#include "Mass/ArcAreaFragments.h"
#include "ArcAreaTypes.h"

bool FArcGovernorAssignWorkersTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(SettlementHandle);
    return true;
}

EStateTreeRunStatus FArcGovernorAssignWorkersTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    checkf(IsInGameThread(), TEXT("Governor tasks must run on game thread for cross-entity access"));
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    FMassEntityManager& EntityManager = MassContext.GetEntityManager();
    const FMassEntityHandle SettlementEntity = MassContext.GetEntity();

    UWorld* World = Context.GetWorld();
    UArcSettlementSubsystem* SettlementSub = World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;
    UArcAreaSubsystem* AreaSubsystem = World ? World->GetSubsystem<UArcAreaSubsystem>() : nullptr;
    if (!SettlementSub || !AreaSubsystem)
    {
        return EStateTreeRunStatus::Failed;
    }

    // Collect idle NPCs
    struct FIdleNPC
    {
        FMassEntityHandle Handle;
        FVector Location;
    };
    TArray<FIdleNPC> IdleNPCs;

    const TArray<FMassEntityHandle>& NPCHandles = SettlementSub->GetNPCsForSettlement(SettlementEntity);
    for (const FMassEntityHandle& NPCHandle : NPCHandles)
    {
        if (!EntityManager.IsEntityValid(NPCHandle))
        {
            continue;
        }
        const FArcEconomyNPCFragment* NPC = EntityManager.GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
        if (NPC && NPC->Role == EArcEconomyNPCRole::Idle)
        {
            const FTransformFragment* Transform = EntityManager.GetFragmentDataPtr<FTransformFragment>(NPCHandle);
            FVector Loc = Transform ? Transform->GetTransform().GetLocation() : FVector::ZeroVector;
            IdleNPCs.Add({NPCHandle, Loc});
        }
    }

    if (IdleNPCs.Num() == 0)
    {
        return EStateTreeRunStatus::Succeeded;
    }

    // Collect vacant area slots from unstaffed building production slots
    struct FVacantSlot
    {
        FMassEntityHandle BuildingHandle;
        FArcAreaSlotHandle AreaSlotHandle;
        FVector Location;
    };
    TArray<FVacantSlot> VacantSlots;

    const TArray<FMassEntityHandle>& BuildingHandles = SettlementSub->GetBuildingsForSettlement(SettlementEntity);
    for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }
        const FArcBuildingFragment* Building = EntityManager.GetFragmentDataPtr<FArcBuildingFragment>(BuildingHandle);
        const FArcBuildingWorkforceFragment* Workforce = EntityManager.GetFragmentDataPtr<FArcBuildingWorkforceFragment>(BuildingHandle);
        const FArcAreaFragment* AreaFrag = EntityManager.GetFragmentDataPtr<FArcAreaFragment>(BuildingHandle);
        if (!Building || !Workforce || !AreaFrag)
        {
            continue;
        }

        const FArcAreaData* AreaData = AreaSubsystem->GetAreaData(AreaFrag->AreaHandle);
        if (!AreaData)
        {
            continue;
        }

        int32 AreaSlotBase = 0;
        for (int32 SlotIndex = 0; SlotIndex < Workforce->Slots.Num(); ++SlotIndex)
        {
            const FArcBuildingSlotData& Slot = Workforce->Slots[SlotIndex];
            if (!Slot.DesiredRecipe)
            {
                AreaSlotBase += Slot.RequiredWorkerCount;
                continue;
            }

            // Add each vacant area slot individually so we fill them one NPC at a time
            for (int32 AreaSlotIdx = AreaSlotBase; AreaSlotIdx < AreaSlotBase + Slot.RequiredWorkerCount; ++AreaSlotIdx)
            {
                const FArcAreaSlotHandle AreaSlotHandle(AreaFrag->AreaHandle, AreaSlotIdx);
                if (AreaSubsystem->GetSlotState(AreaSlotHandle) == EArcAreaSlotState::Vacant)
                {
                    VacantSlots.Add({BuildingHandle, AreaSlotHandle, Building->BuildingLocation});
                }
            }

            AreaSlotBase += Slot.RequiredWorkerCount;
        }
    }

    // Assign idle NPCs to vacant slots -- greedy, nearest NPC
    for (const FVacantSlot& Slot : VacantSlots)
    {
        if (IdleNPCs.Num() == 0)
        {
            break;
        }

        // Find nearest idle NPC
        int32 BestNPCIndex = 0;
        float BestDistSq = TNumericLimits<float>::Max();
        for (int32 NPCIndex = 0; NPCIndex < IdleNPCs.Num(); ++NPCIndex)
        {
            const float DistSq = FVector::DistSquared(IdleNPCs[NPCIndex].Location, Slot.Location);
            if (DistSq < BestDistSq)
            {
                BestDistSq = DistSq;
                BestNPCIndex = NPCIndex;
            }
        }

        const FMassEntityHandle NPCHandle = IdleNPCs[BestNPCIndex].Handle;
        const bool bAssigned = AreaSubsystem->AssignToSlot(Slot.AreaSlotHandle, NPCHandle);
        if (bAssigned)
        {
            FArcEconomyNPCFragment* NPC = EntityManager.GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
            if (NPC)
            {
                NPC->Role = EArcEconomyNPCRole::Worker;
            }

            FArcWorkerFragment* Worker = EntityManager.GetFragmentDataPtr<FArcWorkerFragment>(NPCHandle);
            if (Worker)
            {
                Worker->AssignedBuildingHandle = Slot.BuildingHandle;
            }
        }

        IdleNPCs.RemoveAtSwap(BestNPCIndex);
    }

    return EStateTreeRunStatus::Succeeded;
}
