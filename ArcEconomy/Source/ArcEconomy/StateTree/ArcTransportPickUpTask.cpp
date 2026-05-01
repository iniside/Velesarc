// Copyright Lukasz Baran. All Rights Reserved.

#include "StateTree/ArcTransportPickUpTask.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcEconomyTypes.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "Mass/ArcMassItemFragments.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemDefinition.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "MassStateTreeExecutionContext.h"
#include "StateTreeLinker.h"

bool FArcTransportPickUpTask::Link(FStateTreeLinker& Linker)
{
    Linker.LinkExternalData(TransporterFragmentHandle);
    Linker.LinkExternalData(NPCFragmentHandle);
    return true;
}

EStateTreeRunStatus FArcTransportPickUpTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
    FMassStateTreeExecutionContext& MassContext = static_cast<FMassStateTreeExecutionContext&>(Context);
    const FArcTransportPickUpTaskInstanceData& InstanceData = Context.GetInstanceData(*this);
    FMassEntityManager& EntityManager = MassContext.GetEntityManager();

    if (!EntityManager.IsEntityValid(InstanceData.SupplyBuildingHandle))
    {
        return EStateTreeRunStatus::Failed;
    }

    // Access building's output fragment
    FArcCraftOutputFragment* Output = EntityManager.GetFragmentDataPtr<FArcCraftOutputFragment>(InstanceData.SupplyBuildingHandle);
    if (!Output)
    {
        return EStateTreeRunStatus::Failed;
    }

    // Determine matching mode: by item def or by tags
    const bool bMatchByTags = !InstanceData.ItemDefinition && !InstanceData.RequiredTags.IsEmpty();

    // Find the item in output and transfer — iterate backwards for safe removal
    int32 PickedUp = 0;
    for (int32 ItemIndex = Output->OutputItems.Num() - 1; ItemIndex >= 0; --ItemIndex)
    {
        FArcItemSpec& ItemSpec = Output->OutputItems[ItemIndex];
        if (ItemSpec.Amount <= 0)
        {
            continue;
        }

        bool bMatches = false;
        if (bMatchByTags)
        {
            const UArcItemDefinition* ItemDef = ItemSpec.GetItemDefinition();
            if (ItemDef)
            {
                const FArcItemFragment_Tags* TagsFrag = ItemDef->FindFragment<FArcItemFragment_Tags>();
                bMatches = TagsFrag && TagsFrag->AssetTags.HasAll(InstanceData.RequiredTags);
            }
        }
        else
        {
            bMatches = ItemSpec.ItemDefinition == InstanceData.ItemDefinition;
        }

        if (bMatches)
        {
            const int32 ToTake = FMath::Min(static_cast<int32>(ItemSpec.Amount), InstanceData.Quantity - PickedUp);
            ItemSpec.Amount -= static_cast<uint16>(ToTake);
            PickedUp += ToTake;

            if (ItemSpec.Amount <= 0)
            {
                Output->OutputItems.RemoveAt(ItemIndex);
            }

            if (PickedUp >= InstanceData.Quantity)
            {
                break;
            }
        }
    }

    if (PickedUp == 0)
    {
        // Supply gone — race condition, return to seek
        return EStateTreeRunStatus::Failed;
    }

    // Release the reservation on the market supply source
    const FArcEconomyNPCFragment& NPC = Context.GetExternalData(NPCFragmentHandle);
    if (NPC.SettlementHandle.IsValid() && EntityManager.IsEntityValid(NPC.SettlementHandle))
    {
        FArcSettlementMarketFragment* Market = EntityManager.GetFragmentDataPtr<FArcSettlementMarketFragment>(NPC.SettlementHandle);
        if (Market)
        {
            FArcResourceMarketData* ResData = Market->PriceTable.Find(InstanceData.ItemDefinition);
            if (ResData)
            {
                for (FArcMarketSupplySource& Source : ResData->SupplySources)
                {
                    if (Source.BuildingHandle == InstanceData.SupplyBuildingHandle)
                    {
                        Source.ReservedQuantity = FMath::Max(0, Source.ReservedQuantity - PickedUp);
                        break;
                    }
                }
            }
        }
    }

    // Add to NPC cargo
    const FMassEntityHandle NPCHandle = MassContext.GetEntity();
    FArcMassItemSpecArrayFragment* Cargo = EntityManager.GetFragmentDataPtr<FArcMassItemSpecArrayFragment>(NPCHandle);
    if (Cargo)
    {
        FArcItemSpec CargoSpec = FArcItemSpec::NewItem(InstanceData.ItemDefinition, 1, PickedUp);
        Cargo->Items.Add(CargoSpec);
    }

    // Update transporter state
    FArcTransporterFragment& Transporter = Context.GetExternalData(TransporterFragmentHandle);
    Transporter.TaskState = EArcTransporterTaskState::Delivering;
    Transporter.TargetQuantity = PickedUp;

    return EStateTreeRunStatus::Succeeded;
}
