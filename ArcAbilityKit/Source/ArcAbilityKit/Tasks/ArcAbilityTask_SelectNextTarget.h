// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ArcAbilityTask_SelectNextTarget.generated.h"

USTRUCT()
struct FArcAbilityTask_SelectNextTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
	FGameplayTag CandidatesTag;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
	FGameplayTag PotentialTargetTag;

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Categories = "GlobalTargeting"))
	FGameplayTag SelectedTargetTag;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float CycleResetDelay = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float LoseTargetRange = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag DeathTag;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector TargetLocationOffset = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FMassEntityHandle SelectedTarget;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bHasTarget = false;
};

USTRUCT(meta = (DisplayName = "Select Next Target", Category = "Arc|Ability|Targeting"))
struct ARCABILITYKIT_API FArcAbilityTask_SelectNextTarget : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_SelectNextTargetInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
