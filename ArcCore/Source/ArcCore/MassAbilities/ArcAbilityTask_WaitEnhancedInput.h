// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeDelegate.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_WaitEnhancedInput.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT()
struct FArcAbilityTask_WaitEnhancedInputInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UInputAction> InputAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeDelegateDispatcher OnInputPressed;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeDelegateDispatcher OnInputTriggered;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeDelegateDispatcher OnInputReleased;

	int32 PressedHandle = INDEX_NONE;
	int32 TriggeredHandle = INDEX_NONE;
	int32 ReleasedHandle = INDEX_NONE;
};

USTRUCT(DisplayName = "Wait Enhanced Input", Category = "Arc|Ability|Input")
struct ARCCORE_API FArcAbilityTask_WaitEnhancedInput : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_WaitEnhancedInputInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
