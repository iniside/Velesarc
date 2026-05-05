// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Targeting/ArcMassTargetData.h"
#include "GameplayTagContainer.h"
#include "ArcAbilityTask_ApplyEffectsFromItem.generated.h"

USTRUCT()
struct FArcAbilityTask_ApplyEffectsFromItemInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GameplayEffect"))
    FGameplayTag EffectTag;

    UPROPERTY(EditAnywhere, Category = "Input")
    FArcMassTargetDataHandle TargetDataHandle;
};

USTRUCT(DisplayName = "Apply Effects From Item", Category = "Arc|Ability|Effects")
struct ARCMASSITEMSRUNTIME_API FArcAbilityTask_ApplyEffectsFromItem : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FArcAbilityTask_ApplyEffectsFromItemInstanceData::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
