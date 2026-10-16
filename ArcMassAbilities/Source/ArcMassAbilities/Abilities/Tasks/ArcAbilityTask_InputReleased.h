// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Delegates/IDelegateInstance.h"
#include "GameplayTagContainer.h"
#include "StateTreeDelegate.h"

#include "ArcAbilityTask_InputReleased.generated.h"

USTRUCT()
struct ARCMASSABILITIES_API FArcAbilityTask_InputReleasedInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FStateTreeDelegateDispatcher OnInputReleased;

	UPROPERTY(VisibleAnywhere, Category = "Output")
	float Duration = 0.f;

	UPROPERTY()
	bool bSawPress = false;

	UPROPERTY()
	double PressTime = 0.0;

	// Not a UPROPERTY — FDelegateHandle is not reflected. The lambda is fully defensive;
	// an orphaned binding is a no-op (WeakContext.MakeStrongExecutionContext() returns
	// invalid and GetInstanceDataPtr returns null).
	FDelegateHandle SignalHandle;
};

USTRUCT(DisplayName = "Arc Input Released", meta = (Category = "Arc|Ability|Input"))
struct ARCMASSABILITIES_API FArcAbilityTask_InputReleased : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcAbilityTask_InputReleasedInstanceData;

	FArcAbilityTask_InputReleased()
	{
		// This task has no Tick override — it is purely event-driven via a Mass signal
		// lambda that fires AbilityStateTreeTickRequired. Without these flag overrides
		// the per-state tick aggregation forces every-frame ticking, defeating the
		// conditional-tick architecture.
		bShouldCallTick = false;
		bConsideredForScheduling = false;
	}

	virtual bool DrivesScheduledTick() const override { return true; }
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
