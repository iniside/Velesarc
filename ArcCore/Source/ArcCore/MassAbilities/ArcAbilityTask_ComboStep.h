// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_ComboStep.generated.h"

class UAnimMontage;

USTRUCT()
struct FArcAbilityTask_ComboStepInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName ComboSection = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ComboWindowStart = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float ComboWindowEnd = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

USTRUCT(DisplayName = "Combo Step", Category = "Arc|Ability|Animation")
struct ARCCORE_API FArcAbilityTask_ComboStep : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FArcAbilityTask_ComboStepInstanceData::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
