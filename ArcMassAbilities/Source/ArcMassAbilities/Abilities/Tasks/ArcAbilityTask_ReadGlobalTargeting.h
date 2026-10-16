// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "ArcAbilityTask_ReadGlobalTargeting.generated.h"

USTRUCT()
struct FArcAbilityTask_ReadGlobalTargetingInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
    FGameplayTag TargetingTag;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    FArcMassTargetDataHandle TargetDataHandle;
};

USTRUCT(DisplayName = "Read Global Targeting", Category = "Arc|Ability|Targeting")
struct ARCMASSABILITIES_API FArcAbilityTask_ReadGlobalTargeting : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_ReadGlobalTargetingInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
