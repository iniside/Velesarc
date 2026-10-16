// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"

#include "ArcMassQuickBarInputBinding.generated.h"

struct FArcItemData;
struct FMassEntityManager;

/**
 * Input binding configuration for a quickbar slot.
 * Stored in FArcMassQuickBarSlot::InputBind as FInstancedStruct.
 * When a slot is activated, Bind is called. When deactivated, Unbind is called.
 *
 * Default implementations are no-ops; concrete subclasses (e.g. the ability
 * subclass in ArcAbilityKit) implement the actual binding.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Arc Mass Quick Bar Input Bind"))
struct ARCMASSITEMSRUNTIME_API FArcMassQuickBarInputBinding
{
    GENERATED_BODY()

public:
    // The input tag this binding stamps onto the underlying target when bound.
    UPROPERTY(EditDefaultsOnly, Category = "Data", meta = (Categories = "InputTag"))
    FGameplayTag InputTag;

    virtual void Bind(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemData* ItemData) const {}
    virtual void Unbind(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const FArcItemData* ItemData) const {}

    virtual ~FArcMassQuickBarInputBinding() = default;
};
