// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_ListenInputPressed.generated.h"

USTRUCT()
struct FArcAbilityTask_ListenInputPressedInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "InputTag"))
    FGameplayTag InputTag;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    FStateTreeDelegateDispatcher OnInputPressed;
};

USTRUCT(DisplayName = "Listen Input Pressed", Category = "Arc|Ability|Input")
struct ARCMASSABILITIES_API FArcAbilityTask_ListenInputPressed : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcAbilityTask_ListenInputPressedInstanceData;

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FInstanceDataType::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
