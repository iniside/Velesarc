// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Items/ArcItemTypes.h"
#include "Mass/EntityHandle.h"
#include "MVVMViewModelBase.h"

#include "ArcEquipmentSlotViewModel.generated.h"

class UArcItemViewModel;
class UObject;
struct FArcMassEquipmentSlot;

UCLASS(BlueprintType)
class ARCUIMODELS_API UArcEquipmentSlotViewModel : public UMVVMViewModelBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    FGameplayTag SlotId;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    FGameplayTagContainer RequiredTags;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    FGameplayTagContainer IgnoreTags;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    FArcItemId CurrentItemId;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter)
    TObjectPtr<UArcItemViewModel> CurrentItem = nullptr;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = "IsEmpty")
    bool bIsEmpty = true;

    UPROPERTY(BlueprintReadOnly, FieldNotify, Getter = "IsAvailable")
    bool bIsAvailable = true;

    const FGameplayTag& GetSlotId() const { return SlotId; }
    const FGameplayTagContainer& GetRequiredTags() const { return RequiredTags; }
    const FGameplayTagContainer& GetIgnoreTags() const { return IgnoreTags; }
    const FArcItemId& GetCurrentItemId() const { return CurrentItemId; }
    UArcItemViewModel* GetCurrentItem() const { return CurrentItem; }
    bool IsEmpty() const { return bIsEmpty; }
    bool IsAvailable() const { return bIsAvailable; }

    // Initialises immutable slot fields. Called once by the struct-model after construction.
    void InitFromSlot(const FArcMassEquipmentSlot& Slot);

    // Sets the target entity handle. Called by the struct-model during initialization so
    // that RequestEquip / RequestUnequip can build commands with a valid entity.
    void SetEntity(FMassEntityHandle InEntity);

    void SetCurrentItem(const FArcItemId& InItemId, UArcItemViewModel* InItemVM);

    void SetAvailable(bool bInAvailable);

    // Tag-only client-side fit test. Tests the item VM's tags against the slot's RequiredTags
    // and IgnoreTags captured by InitFromSlot. Cheap; safe to call from Blueprint per-frame.
    UFUNCTION(BlueprintCallable, Category = "Arc UI")
    bool CanAccept(UArcItemViewModel* InItem) const;

    // Sends FArcMassEquipFromStoreCommand for Item. The item id and its current item-store
    // slot are read off the VM. WorldContextObject is required so the helper can find the
    // local PlayerController. Returns false if the entity, slot, or Item is invalid.
    UFUNCTION(BlueprintCallable, Category = "Arc UI", meta = (WorldContext = "WorldContextObject"))
    bool RequestEquip(UObject* WorldContextObject, UArcItemViewModel* Item);

    // Sends FArcMassUnequipCommand for the currently equipped item in this slot.
    // Returns false if the entity or slot id is invalid or the slot is already empty.
    UFUNCTION(BlueprintCallable, Category = "Arc UI", meta = (WorldContext = "WorldContextObject"))
    bool RequestUnequip(UObject* WorldContextObject);

private:
    // Set by the struct-model during Initialize. Not replicated; lives on client only.
    FMassEntityHandle TargetEntity;
};
