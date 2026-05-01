// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "ArcAbilityTask_ApplyEffectToTargets.generated.h"

class UArcEffectDefinition;

USTRUCT()
struct FArcAbilityTask_ApplyEffectToTargetsInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UArcEffectDefinition> EffectDefinition = nullptr;

    UPROPERTY(EditAnywhere, Category = "Input")
    FArcMassTargetDataHandle TargetDataHandle;
};

USTRUCT(DisplayName = "Apply Effect To Targets", Category = "Arc|Ability|Effects")
struct ARCCORE_API FArcAbilityTask_ApplyEffectToTargets : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_ApplyEffectToTargetsInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
