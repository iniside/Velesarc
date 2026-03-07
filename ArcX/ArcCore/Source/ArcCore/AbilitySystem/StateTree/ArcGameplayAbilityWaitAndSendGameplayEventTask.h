// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegates.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityWaitAndSendGameplayEventTask.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct FArcGameplayAbilityWaitAndSendGameplayEventTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bWaitTimeOver;

	UPROPERTY(EditAnywhere, Category = Parameter)
	float WaitTime = 0;

	UPROPERTY()
	float CurrentTime = 0;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnWaitEnded;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EventTag;
};

USTRUCT()
struct FArcGameplayAbilityWaitAndSendGameplayEventTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityWaitAndSendGameplayEventTaskInstanceData;

	FArcGameplayAbilityWaitAndSendGameplayEventTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
