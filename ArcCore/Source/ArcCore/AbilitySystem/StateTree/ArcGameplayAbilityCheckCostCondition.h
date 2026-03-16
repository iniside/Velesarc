// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityCheckCostCondition.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcGameplayAbilityCheckCostConditionInstanceData
{
	GENERATED_BODY()

	/** The gameplay ability whose cost requirements to check. */
	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;
};

/**
 * Condition that checks whether the ability's cost can be paid.
 * Returns true if the owning actor has enough resources to cover the ability's cost effect.
 */
USTRUCT(meta = (DisplayName = "Check Ability Cost", Tooltip = "Condition that checks whether the ability's cost can be paid. Returns true if the owning actor has enough resources to cover the ability's cost effect."))
struct FArcGameplayAbilityCheckCostCondition : public FArcGameplayAbilityConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityCheckCostConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};
