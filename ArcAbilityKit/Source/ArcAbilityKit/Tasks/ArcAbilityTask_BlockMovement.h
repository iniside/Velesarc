// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "MovementModifier.h"
#include "ArcAbilityTask_BlockMovement.generated.h"

class UMoverComponent;

USTRUCT()
struct FArcAbilityTask_BlockMovementInstanceData
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<UMoverComponent> MoverComponent;

	UPROPERTY(Transient)
	FMovementModifierHandle ModifierHandle;
};

USTRUCT(DisplayName = "Block Movement", Category = "Arc|Ability|Movement")
struct ARCABILITYKIT_API FArcAbilityTask_BlockMovement : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	FArcAbilityTask_BlockMovement()
	{
		bShouldStateChangeOnReselect = false;
		bShouldCallTick = false;
#if WITH_EDITORONLY_DATA
		bConsideredForCompletion = false;
		bCanEditConsideredForCompletion = false;
#endif
	}

	using FInstanceDataType = FArcAbilityTask_BlockMovementInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;
};
