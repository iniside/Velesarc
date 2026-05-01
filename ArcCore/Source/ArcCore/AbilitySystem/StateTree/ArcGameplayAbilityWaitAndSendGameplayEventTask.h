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

	/** The ability system component to send the gameplay event to. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** True when the wait time has elapsed and the event has been sent. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bWaitTimeOver = false;

	/** Time in seconds to wait before sending the gameplay event. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	float WaitTime = 0;

	UPROPERTY()
	float CurrentTime = 0;

	/** Delegate broadcast when the wait time completes. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnWaitEnded;

	/** Gameplay tag sent as a gameplay event on the ASC when the wait time elapses. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EventTag;
};

/**
 * Waits for a specified duration, then sends a gameplay event with the given tag to the ability system component.
 * Broadcasts OnWaitEnded and sets bWaitTimeOver when the time elapses. Stays Running after sending.
 */
USTRUCT(meta = (DisplayName = "Wait And Send Gameplay Event", Tooltip = "Waits for a specified duration, then sends a gameplay event with the given tag to the ability system component. Broadcasts OnWaitEnded and sets bWaitTimeOver when the time elapses. Stays Running after sending."))
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

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
