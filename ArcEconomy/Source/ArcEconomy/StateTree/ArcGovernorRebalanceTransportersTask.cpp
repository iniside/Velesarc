// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcGovernorRebalanceTransportersTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcEconomyTypes.h"
#include "ArcSettlementSubsystem.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "MassEntityView.h"
#include "Items/ArcItemSpec.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcGovernorRebalanceTransportersTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(SettlementHandle);
    Linker.LinkExternalData(WorkforceHandle);
    return true;
}

EStateTreeRunStatus FArcGovernorRebalanceTransportersTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    checkf(IsInGameThread(), TEXT("Governor tasks must run on game thread for cross-entity access"));
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    FMassEntityManager& EntityManager = MassContext.GetEntityManager();
    const FMassEntityHandle SettlementEntity = MassContext.GetEntity();
    const FArcGovernorRebalanceTransportersInstanceData& InstanceData = Context.GetInstanceData<FArcGovernorRebalanceTransportersInstanceData>(*this);

    UWorld* World = Context.GetWorld();
    UArcSettlementSubsystem* SettlementSub = World ? World->GetSubsystem<UArcSettlementSubsystem>() : nullptr;
    if (!SettlementSub)
    {
        return EStateTreeRunStatus::Failed;
    }

    // Count total output backlog across all buildings
    int32 TotalOutputBacklog = 0;
    const TArray<FMassEntityHandle>& BuildingHandles = SettlementSub->GetBuildingsForSettlement(SettlementEntity);
    for (const FMassEntityHandle& BuildingHandle : BuildingHandles)
    {
        if (!EntityManager.IsEntityValid(BuildingHandle))
        {
            continue;
        }
        const FArcCraftOutputFragment* Output = EntityManager.GetFragmentDataPtr<FArcCraftOutputFragment>(BuildingHandle);
        if (Output)
        {
            for (const FArcItemSpec& Item : Output->OutputItems)
            {
                TotalOutputBacklog += Item.Amount;
            }
        }
    }

    // Count current workers, transporters, idle
    int32 WorkerCount = 0;
    int32 LocalTransporterCount = 0;
    int32 CaravanCount = 0;
    int32 IdleCount = 0;

    const TArray<FMassEntityHandle>& NPCHandles = SettlementSub->GetNPCsForSettlement(SettlementEntity);
    for (const FMassEntityHandle& NPCHandle : NPCHandles)
    {
        if (!EntityManager.IsEntityValid(NPCHandle))
        {
            continue;
        }
        const FArcEconomyNPCFragment* NPC = EntityManager.GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
        if (!NPC)
        {
            continue;
        }
        switch (NPC->Role)
        {
        case EArcEconomyNPCRole::Worker:
            ++WorkerCount;
            break;
        case EArcEconomyNPCRole::Transporter:
        {
            const FMassEntityView NPCView(EntityManager, NPCHandle);
            if (NPCView.HasTag<FArcCaravanTag>())
            {
                ++CaravanCount;
            }
            else
            {
                ++LocalTransporterCount;
            }
            break;
        }
        case EArcEconomyNPCRole::Idle:
            ++IdleCount;
            break;
        }
    }

    // Update workforce fragment
    FArcSettlementWorkforceFragment& Workforce = Context.GetExternalData(WorkforceHandle);
    Workforce.WorkerCount = WorkerCount;
    Workforce.TransporterCount = LocalTransporterCount;
    Workforce.CaravanCount = CaravanCount;
    Workforce.IdleCount = IdleCount;

    // Check if we need more transporters
    const bool bBacklogged = TotalOutputBacklog > InstanceData.OutputBacklogThreshold;
    const float CurrentRatio = LocalTransporterCount > 0 ? static_cast<float>(WorkerCount) / static_cast<float>(LocalTransporterCount) : TNumericLimits<float>::Max();
    const bool bNeedMoreTransporters = bBacklogged || CurrentRatio > InstanceData.TargetWorkerTransporterRatio * 1.5f;

    if (bNeedMoreTransporters && IdleCount > 0)
    {
        // Convert one idle NPC to transporter
        for (const FMassEntityHandle& NPCHandle : NPCHandles)
        {
            if (!EntityManager.IsEntityValid(NPCHandle))
            {
                continue;
            }
            FArcEconomyNPCFragment* NPC = EntityManager.GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
            if (NPC && NPC->Role == EArcEconomyNPCRole::Idle)
            {
                NPC->Role = EArcEconomyNPCRole::Transporter;

                FArcTransporterFragment* Transporter = EntityManager.GetFragmentDataPtr<FArcTransporterFragment>(NPCHandle);
                if (Transporter)
                {
                    Transporter->TaskState = EArcTransporterTaskState::Idle;
                }
                break; // One per tick
            }
        }
    }

    // Recall caravans if local transport is suffering
    if (bBacklogged && LocalTransporterCount == 0 && CaravanCount > 0)
    {
        for (const FMassEntityHandle& NPCHandle : NPCHandles)
        {
            if (!EntityManager.IsEntityValid(NPCHandle))
            {
                continue;
            }
            FArcEconomyNPCFragment* NPC = EntityManager.GetFragmentDataPtr<FArcEconomyNPCFragment>(NPCHandle);
            if (!NPC || NPC->Role != EArcEconomyNPCRole::Transporter)
            {
                continue;
            }

            const FMassEntityView NPCView(EntityManager, NPCHandle);
            const bool bIsCaravan = NPCView.HasTag<FArcCaravanTag>();

            if (bIsCaravan)
            {
                FArcTransporterFragment* Transporter = EntityManager.GetFragmentDataPtr<FArcTransporterFragment>(NPCHandle);
                if (Transporter)
                {
                    Transporter->TaskState = EArcTransporterTaskState::Idle;
                    Transporter->TargetBuildingHandle = FMassEntityHandle();
                }
                break; // One per tick
            }
        }
    }

    return EStateTreeRunStatus::Succeeded;
}
