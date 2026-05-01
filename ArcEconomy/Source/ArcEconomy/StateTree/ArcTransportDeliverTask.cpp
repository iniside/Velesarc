// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcTransportDeliverTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcEconomyTypes.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "Mass/ArcMassItemFragments.h"
#include "Items/ArcItemSpec.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcTransportDeliverTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(TransporterFragmentHandle);
    return true;
}

EStateTreeRunStatus FArcTransportDeliverTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    const FArcTransportDeliverTaskInstanceData& InstanceData = Context.GetInstanceData(*this);
    FMassEntityManager& EntityManager = MassContext.GetEntityManager();

    if (!EntityManager.IsEntityValid(InstanceData.DemandBuildingHandle))
    {
        return EStateTreeRunStatus::Failed;
    }

    // Access building's input fragment
    FArcCraftInputFragment* Input = EntityManager.GetFragmentDataPtr<FArcCraftInputFragment>(InstanceData.DemandBuildingHandle);
    if (!Input)
    {
        return EStateTreeRunStatus::Failed;
    }

    // Transfer cargo from NPC to building input
    const FMassEntityHandle NPCHandle = MassContext.GetEntity();
    FArcMassItemSpecArrayFragment* Cargo = EntityManager.GetFragmentDataPtr<FArcMassItemSpecArrayFragment>(NPCHandle);
    if (!Cargo)
    {
        return EStateTreeRunStatus::Failed;
    }

    for (const FArcItemSpec& CargoItem : Cargo->Items)
    {
        if (CargoItem.Amount <= 0)
        {
            continue;
        }

        // Try to stack with existing input items
        bool bStacked = false;
        for (FArcItemSpec& InputItem : Input->InputItems)
        {
            if (InputItem.ItemDefinition == CargoItem.ItemDefinition)
            {
                InputItem.Amount += CargoItem.Amount;
                bStacked = true;
                break;
            }
        }

        if (!bStacked)
        {
            Input->InputItems.Add(CargoItem);
        }
    }

    // Clear cargo
    Cargo->Items.Empty();

    // Reset transporter state
    FArcTransporterFragment& Transporter = Context.GetExternalData(TransporterFragmentHandle);
    Transporter.TaskState = EArcTransporterTaskState::Idle;
    Transporter.TargetBuildingHandle = FMassEntityHandle();
    Transporter.TargetItemDefinition = nullptr;
    Transporter.TargetQuantity = 0;

    return EStateTreeRunStatus::Succeeded;
}
