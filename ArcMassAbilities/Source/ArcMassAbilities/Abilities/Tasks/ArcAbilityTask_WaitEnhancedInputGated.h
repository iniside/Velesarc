// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeDelegate.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_WaitEnhancedInputGated.generated.h"

class UInputAction;
class UInputMappingContext;

USTRUCT()
struct FArcAbilityTask_WaitEnhancedInputGatedInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UInputAction> InputAction = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	TObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeDelegateDispatcher OnInputPressed;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeDelegateDispatcher OnInputTriggered;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeDelegateDispatcher OnInputReleased;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	int32 MappingContextPriority = 999;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float HeldDuration = 0.f;

	UPROPERTY()
	bool bWasPressed = false;

	double PressTimeSeconds = 0.0;

	UPROPERTY()
	int32 PressedHandle = INDEX_NONE;

	UPROPERTY()
	int32 TriggeredHandle = INDEX_NONE;

	UPROPERTY()
	int32 ReleasedHandle = INDEX_NONE;
};

USTRUCT(DisplayName = "Wait Enhanced Input (Gated)", Category = "Arc|Ability|Input")
struct ARCMASSABILITIES_API FArcAbilityTask_WaitEnhancedInputGated : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	FArcAbilityTask_WaitEnhancedInputGated()
	{
		// This task has no Tick override — it is purely event-driven via three
		// EnhancedInput lambdas that fire AbilityStateTreeTickRequired. Without
		// these flag overrides the per-state tick aggregation sets bCachedRequestTick
		// and GetNextScheduledTick returns MakeEveryFrames before checking scheduled
		// requests, defeating the conditional-tick architecture.
		bShouldCallTick = false;
		bConsideredForScheduling = false;
	}

	virtual bool DrivesScheduledTick() const override { return true; }

	using FInstanceDataType = FArcAbilityTask_WaitEnhancedInputGatedInstanceData;

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
