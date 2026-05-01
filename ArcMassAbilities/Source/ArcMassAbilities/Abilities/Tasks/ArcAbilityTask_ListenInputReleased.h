// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_ListenInputReleased.generated.h"

USTRUCT()
struct FArcAbilityTask_ListenInputReleasedInstanceData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "InputTag"))
    FGameplayTag InputTag;

    UPROPERTY(EditAnywhere, Category = "Parameter")
    FStateTreeDelegateDispatcher OnInputReleased;
};

USTRUCT(DisplayName = "Listen Input Released", Category = "Arc|Ability|Input")
struct ARCMASSABILITIES_API FArcAbilityTask_ListenInputReleased : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcAbilityTask_ListenInputReleasedInstanceData;

    virtual const UStruct* GetInstanceDataType() const override
    {
        return FInstanceDataType::StaticStruct();
    }

    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
    virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};
