// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcGovernorRebalanceTransportersTask.generated.h"

USTRUCT()
struct FArcGovernorRebalanceTransportersInstanceData
{
    GENERATED_BODY()

    /** From UArcGovernorDataAsset. */
    UPROPERTY(EditAnywhere, Category = "Config")
    float TargetWorkerTransporterRatio = 4.0f;

    UPROPERTY(EditAnywhere, Category = "Config")
    int32 OutputBacklogThreshold = 20;
};

/**
 * Governor task: adjusts worker:transporter ratio based on output backlogs.
 * Also recalls caravans to local duty if local transport is falling behind.
 */
USTRUCT(DisplayName = "Arc Governor Rebalance Transporters", meta = (Category = "Economy|Governor"))
struct ARCECONOMY_API FArcGovernorRebalanceTransportersTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcGovernorRebalanceTransportersInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
    TStateTreeExternalDataHandle<FArcSettlementFragment> SettlementHandle;
    TStateTreeExternalDataHandle<FArcSettlementWorkforceFragment> WorkforceHandle;
};
