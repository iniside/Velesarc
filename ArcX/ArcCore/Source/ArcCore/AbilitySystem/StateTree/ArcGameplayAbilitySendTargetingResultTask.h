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

	UPROPERTY(EditAnywhere, Category = Input)
	TObjectPtr<UGameplayAbility> Ability;

	UPROPERTY(EditAnywhere, Category = Input)
	FGameplayAbilityTargetDataHandle TargetData;

	/** If null, reads from item's FArcItemFragment_TargetingObjectPreset. */
	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<UArcTargetingObject> TargetingObject;
};

USTRUCT(meta = (DisplayName = "Send Targeting Result"))
struct FArcGameplayAbilitySendTargetingResultTask : public FArcGameplayAbilityTaskBase
{
	GENERATED_BODY()

public:
	using FInstanceDataType = FArcGameplayAbilitySendTargetingResultTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
