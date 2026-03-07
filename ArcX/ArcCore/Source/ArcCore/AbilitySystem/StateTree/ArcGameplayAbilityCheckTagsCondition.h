// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityCheckTagsCondition.generated.h"

class UAbilitySystemComponent;

USTRUCT()
struct FArcGameplayAbilityCheckTagsConditionInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTagContainer BlockedTags;
};

USTRUCT(meta = (DisplayName = "Check Gameplay Tags"))
struct FArcGameplayAbilityCheckTagsCondition : public FArcGameplayAbilityConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGameplayAbilityCheckTagsConditionInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
