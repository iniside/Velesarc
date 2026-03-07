// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeDelegates.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityWaitInputTask.generated.h"

class UAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;

USTRUCT()
struct FArcGameplayAbilityWaitInputTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnInputTriggered;

	UPROPERTY()
	int32 InputHandle;

	UPROPERTY()
	int32 ReleasedHandle;
};

USTRUCT()
struct FArcGameplayAbilityWaitInputTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityWaitInputTaskInstanceData;

	FArcGameplayAbilityWaitInputTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
