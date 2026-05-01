// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "ArcAbilityTask_ExecuteGlobalTargeting.generated.h"

USTRUCT()
struct FArcAbilityTask_ExecuteGlobalTargetingInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
    FGameplayTag TargetingTag;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    FArcMassTargetDataHandle TargetDataHandle;
};

USTRUCT(DisplayName = "Execute Global Targeting", Category = "Arc|Ability|Targeting")
struct ARCCORE_API FArcAbilityTask_ExecuteGlobalTargeting : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_ExecuteGlobalTargetingInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
