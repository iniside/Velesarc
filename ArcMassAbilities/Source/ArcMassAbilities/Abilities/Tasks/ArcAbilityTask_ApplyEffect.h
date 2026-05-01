// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_ApplyEffect.generated.h"

class UArcEffectDefinition;

USTRUCT(DisplayName = "Apply Effect", Category = "Arc|Ability")
struct ARCMASSABILITIES_API FArcAbilityTask_ApplyEffect : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter")
    TObjectPtr<UArcEffectDefinition> EffectDefinition = nullptr;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    bool bApplyToSelf = true;

    virtual const UStruct* GetInstanceDataType() const override { return nullptr; }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
