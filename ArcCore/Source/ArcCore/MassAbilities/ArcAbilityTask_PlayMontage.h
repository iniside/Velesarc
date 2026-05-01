// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_PlayMontage.generated.h"

class UAnimMontage;

USTRUCT()
struct FArcAbilityTask_PlayMontageInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float PlayRate = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ToolTip = "Override duration. 0 = use montage length."))
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FName StartSection = NAME_None;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	bool bLoop = false;
};

USTRUCT(DisplayName = "Play Montage", Category = "Arc|Ability|Animation")
struct ARCCORE_API FArcAbilityTask_PlayMontage : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FArcAbilityTask_PlayMontageInstanceData::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
