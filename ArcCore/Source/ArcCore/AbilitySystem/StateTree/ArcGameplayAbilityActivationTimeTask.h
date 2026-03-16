// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreePropertyFunctionBase.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "UObject/Object.h"
#include "ArcGameplayAbilityActivationTimeTask.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;

USTRUCT()
struct FArcGameplayAbilityActivationTimeTaskInstanceData
{
	GENERATED_BODY()

	/** The ability system component owning the ability. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** The gameplay ability whose activation time to track. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** True when the activation time has fully elapsed. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bWaitTimeOver = false;

	/** Elapsed time since the task started, in seconds. */
	UPROPERTY(VisibleAnywhere, Category = Output)
	float CurrentTime = 0;

	/** Delegate broadcast when activation time completes. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnActivationCompleted;

	/** Gameplay tag sent as event when activation completes. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EventTag;
};

/**
 * Waits for the ability's activation time to elapse, then broadcasts a delegate.
 * The activation time is read from UArcCoreGameplayAbility::GetActivationTime().
 * Runs until activation time passes, then stays Running with bWaitTimeOver set.
 */
USTRUCT(meta = (DisplayName = "Wait Activation Time", Tooltip = "Waits for the ability's activation time to elapse, then broadcasts a delegate. The activation time is read from UArcCoreGameplayAbility::GetActivationTime(). Runs until activation time passes, then stays Running with bWaitTimeOver set."))
struct FArcGameplayAbilityActivationTimeTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityActivationTimeTaskInstanceData;

	FArcGameplayAbilityActivationTimeTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

USTRUCT()
struct FArcGameplayAbilityGetActivationTimePropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** The gameplay ability to read activation time from. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** The activation time value read from the ability. */
	UPROPERTY(EditAnywhere, Category = Output)
	float ActivationTime;
};

/**
 * Property function that reads the activation time from a UArcCoreGameplayAbility.
 */
USTRUCT(meta = (DisplayName = "Get Activation Time", Tooltip = "Property function that reads the activation time from a UArcCoreGameplayAbility."))
struct FArcGameplayAbilityGetActivationTimePropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityGetActivationTimePropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

USTRUCT()
struct FArcGameplayAbilityGetItemScalableFloatPropertyFunctionInstanceData
{
	GENERATED_BODY()

	/** The item gameplay ability to read scalable float data from. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** The scalable float fragment struct type to look up on the item. */
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (MetaStruct = "/Script/ArcCore.ArcScalableFloatItemFragment"))
	TObjectPtr<UScriptStruct> ScalableFloatStruct;

	/** The name of the scalable float value within the fragment. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName ScalableFloatName;

	/** The evaluated scalable float value. */
	UPROPERTY(EditAnywhere, Category = Output)
	float Value;
};

/**
 * Property function that reads a scalable float value from an item's fragment data.
 * Requires a UArcItemGameplayAbility.
 */
USTRUCT(meta = (DisplayName = "Get Item Scalable Float", Tooltip = "Property function that reads a scalable float value from an item's fragment data. Requires a UArcItemGameplayAbility."))
struct FArcGameplayAbilityGetItemScalableFloatPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityGetItemScalableFloatPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};