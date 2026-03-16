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

	/** The ability system component providing avatar actor and context for targeting. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** The item gameplay ability that provides the targeting context and item definition. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** Targeting object to execute. If null, reads from the item's FArcItemFragment_TargetingObjectPreset. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UArcTargetingObject> TargetingObject;

	/** Output target data handle containing the targeting results as GAS target data. */
	UPROPERTY(EditAnywhere, Category = Output)
	FGameplayAbilityTargetDataHandle TargetDataHandle;

	/** Output array of raw hit results from the targeting request. */
	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FHitResult> HitResults;

	/** True if the targeting request found at least one valid result. */
	UPROPERTY(EditAnywhere, Category = Output)
	bool bHasResults = false;

	UPROPERTY()
	FTargetingRequestHandle RequestHandle;

	UPROPERTY()
	bool bRequestComplete = false;
};

/**
 * Executes an async targeting request using the Targeting System.
 * Resolves the targeting preset from the provided TargetingObject or the item's FArcItemFragment_TargetingObjectPreset.
 * Runs until the targeting request completes, then outputs hit results and GAS target data.
 * Cancels any pending request if the state is exited early.
 */
USTRUCT(meta = (DisplayName = "Execute Targeting", Tooltip = "Executes an async targeting request using the Targeting System. Resolves the targeting preset from the provided TargetingObject or the item's FArcItemFragment_TargetingObjectPreset. Runs until the targeting request completes, then outputs hit results and GAS target data. Cancels any pending request if the state is exited early."))
struct FArcGameplayAbilityExecuteTargetingTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityExecuteTargetingTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
