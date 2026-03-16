// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityEndAbilityTask.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;

USTRUCT()
struct FArcGameplayAbilityEndAbilityTaskInstanceData
{
	GENERATED_BODY()

	/** The gameplay ability to end. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** The ability system component owning the ability. Used to call the server end ability RPC. */
	UPROPERTY(EditAnywhere, Category = Output)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** If true, the ability is ended as cancelled rather than completed normally. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bWasCancelled = false;
};

/**
 * Ends the gameplay ability by calling CallServerEndAbility on the ASC.
 * Completes immediately with Succeeded on success, Failed if the ability is null.
 */
USTRUCT(meta = (DisplayName = "End Ability", Tooltip = "Ends the gameplay ability by calling CallServerEndAbility on the ASC. Completes immediately with Succeeded on success, Failed if the ability is null."))
struct FArcGameplayAbilityEndAbilityTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityEndAbilityTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
