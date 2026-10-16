// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/ArcMassAbilityCost.h"
#include "ArcAbilityTask_Commit.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityTask_Commit_InstanceData
{
    GENERATED_BODY()

    // How to handle a cannot-afford result: FailState returns Failed (hard gate);
    // HandleInTree returns Succeeded with bCommitted=false so the tree can branch.
    UPROPERTY(EditAnywhere, Category = "Parameter")
    EArcGateHandling Handling = EArcGateHandling::HandleInTree;

    // True when both cost and cooldown were applied successfully.
    UPROPERTY(VisibleAnywhere, Category = "Output")
    bool bCommitted = false;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    bool bCostApplied = false;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    bool bCooldownApplied = false;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float AppliedCooldownDuration = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    bool bCanAffordCost = true;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    float MissingAmount = 0.f;

    UPROPERTY(VisibleAnywhere, Category = "Output")
    EArcCostFailReason Reason = EArcCostFailReason::Insufficient;
};

USTRUCT(DisplayName = "Commit Ability", Category = "Arc|Ability")
struct ARCMASSABILITIES_API FArcAbilityTask_Commit : public FArcAbilityTaskBase
{
    GENERATED_BODY()

    using FInstanceDataType = FArcAbilityTask_Commit_InstanceData;
    virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
    virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
