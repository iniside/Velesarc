// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityApplyCooldownTask.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcGameplayAbilityApplyCooldownTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;
};

USTRUCT(meta = (DisplayName = "Apply Ability Cooldown"))
struct FArcGameplayAbilityApplyCooldownTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityApplyCooldownTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
