// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcTransportPickUpTask.generated.h"

class UArcItemDefinition;

USTRUCT()
struct FArcTransportPickUpTaskInstanceData
{
    GENERATED_BODY()

    /** Input: building to pick up from. */
    UPROPERTY(EditAnywhere, Category = "Input")
    FMassEntityHandle SupplyBuildingHandle;

    /** Input: item to pick up (may be null for tag-based demands). */
    UPROPERTY(EditAnywhere, Category = "Input")
    TObjectPtr<UArcItemDefinition> ItemDefinition = nullptr;

    /** Input: for tag-based demands, match output items by tags instead of item def. */
    UPROPERTY(EditAnywhere, Category = "Input", meta = (Optional))
    FGameplayTagContainer RequiredTags;

    /** Input: quantity to pick up. */
    UPROPERTY(EditAnywhere, Category = "Input")
    int32 Quantity = 0;
};

/**
 * Transfers items from building's FArcCraftOutputFragment to NPC's FArcMassItemSpecArrayFragment.
 * If supply is gone on arrival (race condition), returns Failed.
 */
USTRUCT(DisplayName = "Arc Transport PickUp Task", meta = (Category = "Economy"))
struct ARCECONOMY_API FArcTransportPickUpTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcTransportPickUpTaskInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
    TStateTreeExternalDataHandle<FArcTransporterFragment> TransporterFragmentHandle;
    TStateTreeExternalDataHandle<FArcEconomyNPCFragment> NPCFragmentHandle;
};
