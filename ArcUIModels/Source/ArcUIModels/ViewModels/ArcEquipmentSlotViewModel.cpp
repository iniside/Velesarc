// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcEquipmentSlotViewModel.h"

#include "Commands/ArcMassEquipFromStoreCommand.h"
#include "Commands/ArcMassUnequipCommand.h"
#include "Commands/ArcReplicatedCommandHelpers.h"
#include "Equipment/ArcMassEquipmentTypes.h"
#include "ViewModels/ArcItemViewModel.h"

namespace ArcUIModelsEquipmentSlotVM
{
    static bool TagsFitSlot(const FGameplayTagContainer& RequiredTags,
                            const FGameplayTagContainer& IgnoreTags,
                            const FGameplayTagContainer& ItemTags)
    {
        if (RequiredTags.Num() > 0 && !ItemTags.HasAll(RequiredTags))
        {
            return false;
        }
        if (IgnoreTags.Num() > 0 && ItemTags.HasAny(IgnoreTags))
        {
            return false;
        }
        return true;
    }
}

void UArcEquipmentSlotViewModel::InitFromSlot(const FArcMassEquipmentSlot& Slot)
{
    UE_MVVM_SET_PROPERTY_VALUE(SlotId, Slot.SlotId);
    UE_MVVM_SET_PROPERTY_VALUE(RequiredTags, Slot.RequiredTags);
    UE_MVVM_SET_PROPERTY_VALUE(IgnoreTags, Slot.IgnoreTags);
}

void UArcEquipmentSlotViewModel::SetEntity(FMassEntityHandle InEntity)
{
    TargetEntity = InEntity;
}

void UArcEquipmentSlotViewModel::SetCurrentItem(const FArcItemId& InItemId, UArcItemViewModel* InItemVM)
{
    UE_MVVM_SET_PROPERTY_VALUE(CurrentItemId, InItemId);

    TObjectPtr<UArcItemViewModel> NewItem = InItemVM;
    UE_MVVM_SET_PROPERTY_VALUE(CurrentItem, NewItem);

    const bool bNewEmpty = !InItemId.IsValid();
    UE_MVVM_SET_PROPERTY_VALUE(bIsEmpty, bNewEmpty);
}

void UArcEquipmentSlotViewModel::SetAvailable(bool bInAvailable)
{
    UE_MVVM_SET_PROPERTY_VALUE(bIsAvailable, bInAvailable);
}

bool UArcEquipmentSlotViewModel::CanAccept(UArcItemViewModel* InItem) const
{
    if (InItem == nullptr)
    {
        return false;
    }
    return ArcUIModelsEquipmentSlotVM::TagsFitSlot(RequiredTags, IgnoreTags, InItem->GetTags());
}

bool UArcEquipmentSlotViewModel::RequestEquip(UObject* WorldContextObject, UArcItemViewModel* Item)
{
    if (WorldContextObject == nullptr || Item == nullptr || !SlotId.IsValid() || !Item->GetItemId().IsValid() || !TargetEntity.IsSet())
    {
        return false;
    }

    return Arcx::SendServerCommand<FArcMassEquipFromStoreCommand>(
        WorldContextObject,
        TargetEntity,
        Item->GetItemId(),
        Item->GetCurrentItemStoreSlot(),
        SlotId);
}

bool UArcEquipmentSlotViewModel::RequestUnequip(UObject* WorldContextObject)
{
    if (WorldContextObject == nullptr || !SlotId.IsValid() || !CurrentItemId.IsValid() || !TargetEntity.IsSet())
    {
        return false;
    }

    return Arcx::SendServerCommand<FArcMassUnequipCommand>(
        WorldContextObject,
        TargetEntity,
        SlotId);
}
