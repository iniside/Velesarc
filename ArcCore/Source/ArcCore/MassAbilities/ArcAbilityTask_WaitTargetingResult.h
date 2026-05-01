// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcAbilityTask_WaitTargetingResult.generated.h"

class UTargetingPreset;

USTRUCT()
struct FArcAbilityTask_WaitTargetingResultInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UTargetingPreset> TargetingPreset = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    FArcMassTargetDataHandle TargetDataHandle;

    FTargetingRequestHandle RequestHandle;
};

USTRUCT(DisplayName = "Wait Targeting Result", Category = "Arc|Ability|Targeting")
struct ARCCORE_API FArcAbilityTask_WaitTargetingResult : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_WaitTargetingResultInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
