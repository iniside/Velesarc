// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityEndAbilityTask.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;

USTRUCT()
struct FArcGameplayAbilityEndAbilityTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bWasCancelled = false;
};

USTRUCT(meta = (DisplayName = "End Ability"))
struct FArcGameplayAbilityEndAbilityTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityEndAbilityTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
