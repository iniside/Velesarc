// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "StateTreeDelegate.h"
#include "ArcAbilityTask_WatchMoverMovement.generated.h"

class UMoverComponent;

USTRUCT()
struct FArcAbilityTask_WatchMoverMovementInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float SpeedThreshold = 1.f;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnMovementStarted;

	UPROPERTY(EditAnywhere, Category = "Output")
	FStateTreeDelegateDispatcher OnMovementStopped;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	bool bIsMoving = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<UMoverComponent> MoverComponent;
};

USTRUCT(DisplayName = "Watch Mover Movement", Category = "Arc|Ability|Movement")
struct ARCABILITYKIT_API FArcAbilityTask_WatchMoverMovement : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_WatchMoverMovementInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
