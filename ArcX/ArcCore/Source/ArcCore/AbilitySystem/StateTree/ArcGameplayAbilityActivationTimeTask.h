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

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;
	
	UPROPERTY(EditAnywhere, Category = Output)
	bool bWaitTimeOver = false;
	
	UPROPERTY(VisibleAnywhere, Category = Output)
	float CurrentTime = 0;
		
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnActivationCompleted;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EventTag;
};

USTRUCT()
struct FArcGameplayAbilityActivationTimeTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcGameplayAbilityActivationTimeTaskInstanceData;

	FArcGameplayAbilityActivationTimeTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT()
struct FArcGameplayAbilityGetActivationTimePropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, Category = Output)
	float ActivationTime;
};

USTRUCT()
struct FArcGameplayAbilityGetActivationTimePropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityGetActivationTimePropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};

USTRUCT()
struct FArcGameplayAbilityGetItemScalableFloatPropertyFunctionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (MetaStruct = "/Script/ArcCore.ArcScalableFloatItemFragment"))
	TObjectPtr<UScriptStruct> ScalableFloatStruct;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName ScalableFloatName;
	
	UPROPERTY(EditAnywhere, Category = Output)
	float Value;
};

USTRUCT()
struct FArcGameplayAbilityGetItemScalableFloatPropertyFunction : public FStateTreePropertyFunctionCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityGetItemScalableFloatPropertyFunctionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual void Execute(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	//virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const;
#endif
};