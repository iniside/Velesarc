// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilitySendTargetingResultTask.generated.h"

class UGameplayAbility;
class UArcTargetingObject;

USTRUCT()
struct FArcGameplayAbilitySendTargetingResultTaskInstanceData
{
	GENERATED_BODY()

	/** The item gameplay ability to send the targeting result for. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** The target data handle to send to the server as the targeting result. */
	UPROPERTY(EditAnywhere, Category = Input)
	FGameplayAbilityTargetDataHandle TargetData;

	/** Targeting object that defines how to process the result. If null, reads from the item's FArcItemFragment_TargetingObjectPreset. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UArcTargetingObject> TargetingObject;
};

/**
 * Sends the targeting result to the server via the item gameplay ability.
 * Resolves the targeting object from the parameter or the item's FArcItemFragment_TargetingObjectPreset.
 * Completes immediately with Succeeded.
 */
USTRUCT(meta = (DisplayName = "Send Targeting Result", Tooltip = "Sends the targeting result to the server via the item gameplay ability. Resolves the targeting object from the parameter or the item's FArcItemFragment_TargetingObjectPreset. Completes immediately with Succeeded."))
struct FArcGameplayAbilitySendTargetingResultTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilitySendTargetingResultTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
