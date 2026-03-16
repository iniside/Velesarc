// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityApplyCooldownTask.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcGameplayAbilityApplyCooldownTaskInstanceData
{
	GENERATED_BODY()

	/** The gameplay ability whose cooldown to apply. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;
};

/**
 * Applies the ability's cooldown gameplay effect.
 * Completes immediately with Succeeded on success, Failed if the ability is not a UArcCoreGameplayAbility.
 */
USTRUCT(meta = (DisplayName = "Apply Ability Cooldown", Tooltip = "Applies the ability's cooldown gameplay effect. Completes immediately with Succeeded on success, Failed if the ability is not a UArcCoreGameplayAbility."))
struct FArcGameplayAbilityApplyCooldownTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityApplyCooldownTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
