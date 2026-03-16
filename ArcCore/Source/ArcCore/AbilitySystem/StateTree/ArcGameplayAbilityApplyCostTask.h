// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityApplyCostTask.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcGameplayAbilityApplyCostTaskInstanceData
{
	GENERATED_BODY()

	/** The gameplay ability whose cost to apply. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;
};

/**
 * Applies the ability's cost gameplay effect (e.g. consuming mana, stamina, ammo).
 * Completes immediately with Succeeded on success, Failed if the ability is not a UArcCoreGameplayAbility.
 */
USTRUCT(meta = (DisplayName = "Apply Ability Cost", Tooltip = "Applies the ability's cost gameplay effect (e.g. consuming mana, stamina, ammo). Completes immediately with Succeeded on success, Failed if the ability is not a UArcCoreGameplayAbility."))
struct FArcGameplayAbilityApplyCostTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityApplyCostTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
