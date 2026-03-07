// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityApplyCostTask.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcGameplayAbilityApplyCostTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;
};

USTRUCT(meta = (DisplayName = "Apply Ability Cost"))
struct FArcGameplayAbilityApplyCostTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityApplyCostTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
