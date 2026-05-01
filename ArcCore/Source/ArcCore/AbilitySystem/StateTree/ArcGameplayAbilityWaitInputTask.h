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

	/** The ability system component used to resolve the avatar pawn for input binding. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** The input action to listen for. When triggered, broadcasts OnInputTriggered. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputAction> InputAction;

	/** Optional input mapping context added at high priority for the wait duration. Removed on exit. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UInputMappingContext> InputMappingContext;

	/** Delegate broadcast each time the input action is triggered. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnInputTriggered;

	UPROPERTY()
	int32 InputHandle = INDEX_NONE;

	UPROPERTY()
	int32 ReleasedHandle = INDEX_NONE;
};

/**
 * Waits for a specific input action to be triggered, broadcasting OnInputTriggered each time.
 * Optionally adds an input mapping context at high priority for the duration of the task.
 * Stays Running indefinitely. Cleans up input bindings and mapping context on exit.
 */
USTRUCT(meta = (DisplayName = "Wait For Input", Tooltip = "Waits for a specific input action to be triggered, broadcasting OnInputTriggered each time. Optionally adds an input mapping context at high priority for the duration of the task. Stays Running indefinitely. Cleans up input bindings and mapping context on exit."))
struct FArcGameplayAbilityWaitInputTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityWaitInputTaskInstanceData;

	FArcGameplayAbilityWaitInputTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
