// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Types/TargetingSystemTypes.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityExecuteTargetingTask.generated.h"

class UAbilitySystemComponent;
class UGameplayAbility;
class UArcTargetingObject;

USTRUCT()
struct FArcGameplayAbilityExecuteTargetingTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** If null, reads from item's FArcItemFragment_TargetingObjectPreset. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UArcTargetingObject> TargetingObject;

	UPROPERTY(EditAnywhere, Category = Output)
	FGameplayAbilityTargetDataHandle TargetDataHandle;

	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FHitResult> HitResults;

	UPROPERTY(EditAnywhere, Category = Output)
	bool bHasResults = false;

	UPROPERTY()
	FTargetingRequestHandle RequestHandle;

	UPROPERTY()
	bool bRequestComplete = false;
};

USTRUCT(meta = (DisplayName = "Execute Targeting"))
struct FArcGameplayAbilityExecuteTargetingTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityExecuteTargetingTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
