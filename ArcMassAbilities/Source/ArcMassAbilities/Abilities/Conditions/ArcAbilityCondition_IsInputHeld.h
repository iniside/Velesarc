// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/Conditions/ArcAbilityConditionBase.h"
#include "ArcAbilityCondition_IsInputHeld.generated.h"

USTRUCT(DisplayName = "Is Input Held", Category = "Arc|Ability|Input")
struct ARCMASSABILITIES_API FArcAbilityCondition_IsInputHeld : public FArcAbilityConditionBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "InputTag"))
    FGameplayTag InputTag;

    virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
