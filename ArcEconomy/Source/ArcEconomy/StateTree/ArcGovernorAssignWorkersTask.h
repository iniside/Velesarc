// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassStateTreeTypes.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcGovernorAssignWorkersTask.generated.h"

USTRUCT()
struct FArcGovernorAssignWorkersInstanceData
{
    GENERATED_BODY()
};

/**
 * Governor task: assigns idle NPCs to unstaffed building production slots.
 * Prefers buildings closest to the idle NPC (Proximity).
 */
USTRUCT(DisplayName = "Arc Governor Assign Workers", meta = (Category = "Economy|Governor"))
struct ARCECONOMY_API FArcGovernorAssignWorkersTask : public FMassStateTreeTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcGovernorAssignWorkersInstanceData;

    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual bool Link(FStateTreeLinker& Linker) override;
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

protected:
    TStateTreeExternalDataHandle<FArcSettlementFragment> SettlementHandle;
};
