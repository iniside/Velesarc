// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_ApplyCooldown.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityTask_ApplyCooldown_InstanceData
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "Output")
    bool bApplied = false;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float AppliedDuration = 0.f;
};

// One-shot apply task: applies the ability's cooldown on EnterState.
// Use in bGateInStateTree trees, where Commit's cooldown APPLY is skipped.
// Do NOT combine with a cooldown-applying Commit in a non-gated tree, or the
// cooldown applies twice.
USTRUCT(meta = (DisplayName = "Apply Cooldown"))
struct ARCMASSABILITIES_API FArcAbilityTask_ApplyCooldown : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcAbilityTask_ApplyCooldown_InstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
