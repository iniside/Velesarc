// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "ArcTargetingPipelineTypes.h"
#include "ArcAbilityTask_ExecuteTargeting.generated.h"

class UArcTargetingPreset;

USTRUCT()
struct FArcAbilityTask_ExecuteTargetingInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UArcTargetingPreset> TargetingPreset = nullptr;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    FArcMassTargetDataHandle TargetDataHandle;

    FArcTargetingHandle RequestHandle;
};

USTRUCT(DisplayName = "Execute Targeting", Category = "Arc|Ability|Targeting")
struct ARCABILITYKIT_API FArcAbilityTask_ExecuteTargeting : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_ExecuteTargetingInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
