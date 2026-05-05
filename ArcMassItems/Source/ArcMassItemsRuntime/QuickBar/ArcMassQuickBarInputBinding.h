// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ArcMassQuickBarInputBinding.generated.h"

struct FArcItemData;
struct FArcItemInstance_GrantedMassAbilities;
struct FArcActiveAbility;
struct FMassEntityManager;
struct FMassEntityHandle;

/**
 * Input binding configuration for a quickbar slot.
 * Stored in FArcMassQuickBarSlot::InputBind as FInstancedStruct.
 * When a slot is activated, BindInput is called. When deactivated, UnbindInput is called.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Arc Mass Quick Bar Input Bind"))
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarInputBinding
{
    GENERATED_BODY()

public:
    // The input tag to bind to abilities when this slot is activated.
    UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "InputTag"))
    FGameplayTag InputTag;

    virtual ~FArcMassQuickBarInputBinding() = default;
};

namespace ArcMassItems::QuickBar::InputBinding
{
    /**
     * Binds the given InputTag to all Mass abilities granted by the item.
     * Looks up FArcItemInstance_GrantedMassAbilities on the item to find ability handles,
     * then sets their InputTag in FArcAbilityCollectionFragment.
     */
    ARCMASSITEMSRUNTIME_API bool BindInputForItem(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const FArcItemData* ItemData,
        FGameplayTag InputTag);

    /**
     * Unbinds (clears) the InputTag from Mass abilities that were previously bound.
     */
    ARCMASSITEMSRUNTIME_API bool UnbindInputForItem(
        FMassEntityManager& EntityManager,
        FMassEntityHandle Entity,
        const FArcItemData* ItemData,
        FGameplayTag InputTag);
}
