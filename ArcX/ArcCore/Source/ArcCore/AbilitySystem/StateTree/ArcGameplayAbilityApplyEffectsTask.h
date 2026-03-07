// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AbilitySystem/ArcAbilityStateTreeTypes.h"
#include "ArcGameplayAbilityApplyEffectsTask.generated.h"

class UGameplayAbility;

USTRUCT()
struct FArcGameplayAbilityApplyEffectsTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	/** If empty, applies ALL effects from item. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayTag EffectTag;

	UPROPERTY(EditAnywhere, Category = Parameter)
	FGameplayAbilityTargetDataHandle TargetData;

	UPROPERTY(EditAnywhere, Category = Output)
	TArray<FActiveGameplayEffectHandle> AppliedEffects;
};

USTRUCT(meta = (DisplayName = "Apply Effects From Item"))
struct FArcGameplayAbilityApplyEffectsTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilityApplyEffectsTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
