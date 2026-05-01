// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcAbilityTask_ExecuteTargeting.generated.h"

class UTargetingPreset;

USTRUCT()
struct FArcAbilityTask_ExecuteTargetingInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UTargetingPreset> TargetingPreset = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    FArcMassTargetDataHandle TargetDataHandle;

    FTargetingRequestHandle RequestHandle;
};

USTRUCT(DisplayName = "Execute Targeting", Category = "Arc|Ability|Targeting")
struct ARCCORE_API FArcAbilityTask_ExecuteTargeting : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_ExecuteTargetingInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
