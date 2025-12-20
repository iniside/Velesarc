// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "UObject/Object.h"
#include "ArcGameplayAbilityPlayMontageTask.generated.h"

class UAbilitySystemComponent;
class UAnimMontage;

/**
 * 
 */
USTRUCT()
struct FArcGameplayAbilityPlayMontageTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UAnimMontage> MontageToPlay;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FStateTreeDelegateDispatcher OnMontageEnded;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FName SectionName;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EndEventTag;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bGatherNotifies;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	float EndBlendTime = 0.1;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUseCustomDuration = false;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Duration = 0;
	
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bIsLooping = false;
	
	UPROPERTY()
	TArray<FArcNotifyEvent> NotifyEvents;
	
	UPROPERTY()
	float CurrentTime;
	
	UPROPERTY()
	float PlayedMontageLength;
	
	FOnMontageEnded MontageEndedDelegate;
	FOnMontageSectionChanged SectionChangedDelegate;
};

USTRUCT()
struct FArcGameplayAbilityPlayMontageTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()
	
public:
	using FInstanceDataType = FArcGameplayAbilityPlayMontageTaskInstanceData;

	FArcGameplayAbilityPlayMontageTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};