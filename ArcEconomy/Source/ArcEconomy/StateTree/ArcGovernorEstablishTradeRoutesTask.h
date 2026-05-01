// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/EntityFragments.h"
#include "ArcGovernorEstablishTradeRoutesTask.generated.h"

USTRUCT()
struct FArcGovernorEstablishTradeRoutesInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Config")
    float TradePriceDifferentialThreshold = 2.0f;
};

/**
 * Governor task: designates idle NPCs as caravans when profitable
 * trade opportunities exist with neighbor settlements.
 * Only allocates caravans if local transport needs are met first.
 */
USTRUCT(DisplayName = "Arc Governor Establish Trade Routes", meta = (Category = "Economy|Governor"))
struct ARCECONOMY_API FArcGovernorEstablishTradeRoutesTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcGovernorEstablishTradeRoutesInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
    TStateTreeExternalDataHandle<FArcSettlementFragment> SettlementHandle;
    TStateTreeExternalDataHandle<FArcSettlementMarketFragment> MarketHandle;
    TStateTreeExternalDataHandle<FArcSettlementWorkforceFragment> WorkforceHandle;
    TStateTreeExternalDataHandle<FTransformFragment> TransformHandle;
};
