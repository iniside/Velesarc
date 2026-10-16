// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "StateTreeExecutionTypes.h"
#include "ArcAbilityTask_WaitDuration.generated.h"

USTRUCT()
struct FArcAbilityTask_WaitDurationInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    float Duration = 1.0f;

    float Elapsed = 0.f;
    UE::StateTree::FScheduledTickHandle ScheduledTickHandle;
};

USTRUCT(DisplayName = "Wait Duration", Category = "Arc|Ability")
struct ARCMASSABILITIES_API FArcAbilityTask_WaitDuration : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcAbilityTask_WaitDurationInstanceData;

    FArcAbilityTask_WaitDuration()
    {
        // Drive scheduling via AddScheduledTickRequest (in EnterState). Without this,
        // GetNextScheduledTick short-circuits to MakeEveryFrames because the state's
        // bCachedRequestTick is set from bShouldCallTick && bConsideredForScheduling.
        bConsideredForScheduling = false;
    }

    virtual bool DrivesScheduledTick() const override { return true; }

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FInstanceDataType::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
