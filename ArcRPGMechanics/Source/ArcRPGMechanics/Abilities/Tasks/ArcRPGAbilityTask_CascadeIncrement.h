// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcRPGAbilityTask_CascadeIncrement.generated.h"

USTRUCT()
struct FArcRPGAbilityTask_CascadeIncrement_InstanceData
{
	GENERATED_BODY()
};

// Commit Ability must run before this task: the new stack only affects the next activation's energy cost.
USTRUCT(DisplayName = "Cascade: Increment", Category = "Arc|Ability|RPG")
struct ARCRPGMECHANICS_API FArcRPGAbilityTask_CascadeIncrement : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcRPGAbilityTask_CascadeIncrement_InstanceData;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
