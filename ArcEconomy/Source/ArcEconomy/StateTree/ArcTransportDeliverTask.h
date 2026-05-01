// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcTransportDeliverTask.generated.h"

USTRUCT()
struct FArcTransportDeliverTaskInstanceData
{
    GENERATED_BODY()

    /** Input: building to deliver to. */
    UPROPERTY(EditAnywhere, Category = "Input")
    FMassEntityHandle DemandBuildingHandle;
};

/**
 * Transfers all cargo from NPC's FArcMassItemSpecArrayFragment to building's FArcCraftInputFragment.
 * Clears NPC cargo and resets transporter state.
 */
USTRUCT(DisplayName = "Arc Transport Deliver Task", meta = (Category = "Economy"))
struct ARCECONOMY_API FArcTransportDeliverTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcTransportDeliverTaskInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
    TStateTreeExternalDataHandle<FArcTransporterFragment> TransporterFragmentHandle;
};
