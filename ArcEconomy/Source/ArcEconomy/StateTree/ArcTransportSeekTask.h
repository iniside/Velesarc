// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcTransportSeekTask.generated.h"

class UArcItemDefinition;

USTRUCT()
struct FArcTransportSeekTaskInstanceData
{
    GENERATED_BODY()

    /** Output: building to pick up from. */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    FMassEntityHandle SupplyBuildingHandle;

    /** Output: building to deliver to. */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    FMassEntityHandle DemandBuildingHandle;

    /** Output: item to transport. */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    TObjectPtr<UArcItemDefinition> ItemDefinition = nullptr;

    /** Output: quantity to transport. */
    UPROPERTY(VisibleAnywhere, Category = "Output")
    int32 Quantity = 0;
};

/**
 * Queries ArcKnowledge for ResourceDemand entries, matches with ResourceSupply.
 * Local transporters query within settlement bounds.
 * Caravans (with FArcCaravanTag) query cross-settlement and calculate profit.
 */
USTRUCT(DisplayName = "Arc Transport Seek Task", meta = (Category = "Economy"))
struct ARCECONOMY_API FArcTransportSeekTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcTransportSeekTaskInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
    TStateTreeExternalDataHandle<FArcEconomyNPCFragment> NPCFragmentHandle;
    TStateTreeExternalDataHandle<FArcTransporterFragment> TransporterFragmentHandle;
    TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
