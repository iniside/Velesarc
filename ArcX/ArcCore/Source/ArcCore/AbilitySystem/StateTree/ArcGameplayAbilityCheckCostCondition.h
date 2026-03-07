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

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;
};

USTRUCT(meta = (DisplayName = "Check Ability Cost"))
struct FArcGameplayAbilityCheckCostCondition : public FArcGameplayAbilityConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityCheckCostConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
